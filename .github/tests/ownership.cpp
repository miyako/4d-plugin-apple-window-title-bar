#include "4DPlugin-JSON.h"
#include "ARRAY_TEXT.h"
#include "C_TEXT.h"
#include "PrivateTypes.h"
#include "EntryPoints.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>

// Only the engine boundary is simulated; all PA_* functions come from the
// complete, unchanged bundled SDK translation unit.
static void check(bool ok, const char *message) {
    if(!ok) throw std::runtime_error(message);
}

static bool allocatedEmpty;
static bool failGet;
static std::set<PA_Unichar *> strings;
static std::map<PA_Handle, size_t> handles;
struct Ref {
    int owners = 1;
    std::map<CUTF16String, PA_Variable> properties;
};
static std::set<Ref *> refs;
static size_t clears;

static PA_Unistring makeString(const PA_Unichar *text, size_t length) {
    PA_Unistring result = {};
    result.fLength = static_cast<PA_long32>(length);
    if(length || allocatedEmpty) {
        result.fString = new PA_Unichar[length + 1];
        if(length) std::memcpy(result.fString, text, length * sizeof(PA_Unichar));
        result.fString[length] = 0;
        PA_Unichar *pointer = result.fString;
        strings.insert(pointer);
    }
    return result;
}

static CUTF16String textOf(PA_Unistring value) {
    if(!value.fString) {
        check(value.fLength == 0, "null string with nonzero length");
        return {};
    }
    PA_Unichar *pointer = value.fString;
    check(strings.count(pointer), "borrowed string no longer alive");
    check(value.fString[value.fLength] == 0, "missing UTF-16 terminator");
    return CUTF16String(value.fString, value.fLength);
}

static void disposeString(PA_Unistring value) {
    if(value.fString) {
        PA_Unichar *pointer = value.fString;
        check(strings.erase(pointer) == 1, "double/string nonowner disposal");
        delete[] value.fString;
    }
}

static Ref *newRef() {
    Ref *ref = new Ref;
    refs.insert(ref);
    return ref;
}

static void clear(PA_Variable &value);
static void release(void *pointer) {
    if(!pointer) return;
    Ref *ref = static_cast<Ref *>(pointer);
    check(refs.count(ref), "reference use after release");
    if(--ref->owners == 0) {
        for(auto &property : ref->properties) clear(property.second);
        refs.erase(ref);
        delete ref;
    }
}

static PA_Variable copy(PA_Variable value) {
    if(value.fType == eVK_Unistring) {
        CUTF16String text = textOf(value.uValue.fString);
        value.uValue.fString = makeString(text.c_str(), text.size());
    }else if(value.fType == eVK_Object || value.fType == eVK_Collection ||
             value.fType == eVK_Picture) {
        Ref *ref = static_cast<Ref *>(value.uValue.fObject);
        if(ref) {
            check(refs.count(ref), "copy of dead reference");
            ++ref->owners;
        }
    }
    return value;
}

static void clear(PA_Variable &value) {
    ++clears;
    switch(static_cast<unsigned char>(value.fType)) {
        case eVK_Unistring: disposeString(value.uValue.fString); break;
        case eVK_Object:
        case eVK_Collection:
        case eVK_Picture: release(value.uValue.fObject); break;
        case eVK_ArrayUnicode: {
            PA_Unistring *data = *reinterpret_cast<PA_Unistring **>(value.uValue.fArray.fData);
            for(PA_long32 i = 0; i <= value.uValue.fArray.fNbElements; ++i)
                disposeString(data[i]);
            PA_DisposeHandle(value.uValue.fArray.fData);
            break;
        }
        case eVK_Boolean:
        case eVK_Real:
        case eVK_Longint:
        case eVK_Undefined:
        case eVK_Null: break;
        default: throw std::runtime_error("unexpected/unspecified variable cleared");
    }
    value = {};
    value.fType = eVK_Undefined;
}

static void engine(short selector, EngineBlock *eb) {
    switch(selector) {
        case EX_CREATE_UNISTRING: {
            PA_Unichar *text = reinterpret_cast<PA_Unichar *>(eb->fHandle);
            eb->fUniString1 = makeString(text, PA_GetUnicharsLength(text));
            break;
        }
        case EX_DISPOSE_UNISTRING: disposeString(eb->fUniString1); break;
        case EX_VARIABLE_TO_STRING: {
            PA_Variable *value = reinterpret_cast<PA_Variable *>(eb->fHandle);
            check(value->fType == eVK_Unistring, "string getter kind");
            textOf(value->uValue.fString);
            eb->fUniString1 = value->uValue.fString;
            break;
        }
        case EX_SET_UNISTRING: {
            PA_Unichar *text = reinterpret_cast<PA_Unichar *>(eb->fHandle);
            PA_Unistring replacement = makeString(text, PA_GetUnicharsLength(text));
            disposeString(eb->fUniString1);
            eb->fUniString1 = replacement;
            break;
        }
        case EX_CREATE_PICTURE: eb->fPicture = newRef(); break;
        case EX_CLEAR_VARIABLE: clear(*static_cast<PA_Variable *>(eb->fPtr1)); break;
        case EX_GET_OBJ_VALUE: {
            if(failGet) {
                std::memset(eb->fPtr2, 0xA5, sizeof(PA_Variable));
                eb->fError = eER_IncompatibleTypes;
                break;
            }
            Ref *ref = static_cast<Ref *>(eb->fPtr1);
            check(refs.count(ref), "get on dead ref");
            CUTF16String key = textOf(eb->fUniString1);
            *static_cast<PA_Variable *>(eb->fPtr2) = copy(ref->properties.at(key));
            break;
        }
        case EX_SET_OBJ_VALUE: {
            Ref *ref = static_cast<Ref *>(eb->fPtr1);
            check(refs.count(ref), "set on dead ref");
            CUTF16String key = textOf(eb->fUniString1);
            auto old = ref->properties.find(key);
            if(old != ref->properties.end()) clear(old->second);
            ref->properties[key] = copy(*static_cast<PA_Variable *>(eb->fPtr2));
            break;
        }
        case EX_HANDLE_MANAGER: {
            PA_Handle handle = eb->fParam1 == 1 ? nullptr : eb->fHandle;
            switch(eb->fParam1) {
                case 1: {
                    PA_Handle handle = reinterpret_cast<PA_Handle>(new char *);
                    *handle = static_cast<char *>(std::calloc(1, eb->fParam2 ? eb->fParam2 : 1));
                    handles[handle] = eb->fParam2;
                    eb->fHandle = handle;
                    break;
                }
                case 2:
                    check(handles.erase(handle) == 1, "double handle disposal");
                    std::free(*eb->fHandle);
                    delete reinterpret_cast<char **>(eb->fHandle);
                    break;
                case 3:
                    check(handles.count(handle), "resize dead handle");
                    *eb->fHandle = static_cast<char *>(std::realloc(*eb->fHandle, eb->fParam2));
                    check(*eb->fHandle, "fixture allocation failed");
                    handles[handle] = eb->fParam2;
                    break;
                case 4: eb->fParam2 = handles.at(handle); break;
                case 5: eb->fParam3 = reinterpret_cast<sLONG_PTR>(*eb->fHandle); break;
                case 6: break;
                default: throw std::runtime_error("unexpected handle operation");
            }
            break;
        }
        case EX_EXECUTE_COMMAND_BY_ID: {
            PA_Variable **args = static_cast<PA_Variable **>(eb->fPtr1);
            PA_Variable *result = static_cast<PA_Variable *>(eb->fPtr2);
            if(eb->fParam1 == 1231) {
                Ref *ref = static_cast<Ref *>(args[0]->uValue.fObject);
                PA_SetBooleanVariable(result, ref->properties.count(textOf(args[1]->uValue.fString)) != 0);
            }else if(eb->fParam1 == 1225) {
                Ref *source = static_cast<Ref *>(args[0]->uValue.fObject);
                Ref *duplicate = newRef();
                for(auto &property : source->properties)
                    duplicate->properties[property.first] = copy(property.second);
                PA_SetObjectVariable(result, duplicate);
            }else if(eb->fParam1 == 1217) {
                PA_Unichar json[] = {'{','}',0};
                PA_Unistring string = PA_CreateUnistring(json);
                PA_SetStringVariable(result, &string);
            }else {
                throw std::runtime_error("unexpected command");
            }
            break;
        }
        default: throw std::runtime_error("unexpected engine selector " + std::to_string(selector));
    }
}

static CUTF16String keyText() {
    PA_Unichar key[] = {'k',0};
    return CUTF16String(key);
}

static void balanced(const char *defect) {
    check(strings.empty(), defect);
    check(handles.empty(), "handle balance");
    check(refs.empty(), "reference balance");
}

static void setters() {
    const char *utf8[] = {"ASCII", "\xE6\x97\xA5", "\xF0\x9F\x98\x80", ""};
    const wchar_t *wide[] = {L"ASCII", L"\u65E5", L"\U0001F600", L""};
    for(size_t i = 0; i < 4; ++i) {
        CUTF16String expected;
        json_wconv(wide[i], &expected);
        for(int variant = 0; variant < 4; ++variant) {
            Ref *object = newRef();
            if(variant == 0) ob_set_s(object, "k", utf8[i]);
            if(variant == 1) ob_set_s(object, L"k", utf8[i]);
            if(variant == 2) ob_set_a(object, L"k", wide[i]);
            if(variant == 3) ob_set_a(object, L"k", &expected);
            check(textOf(object->properties.at(keyText()).uValue.fString) == expected,
                  "exact setter UTF-16 content/units");
            release(object);
            balanced("overwritten allocated empty string leak");
        }
    }
    Ref *object = newRef();
    ob_set_p(object, L"k", newRef());
    check(refs.size() == 2, "overwritten empty picture leak");
    release(object);
    balanced("picture setter balance");
}

static void getters() {
    const wchar_t *fixtures[] = {L"ASCII", L"\u65E5", L"\U0001F600", L""};
    for(const wchar_t *fixture : fixtures) {
        Ref *object = newRef();
        CUTF16String expected;
        json_wconv(fixture, &expected);
        PA_Variable source = {};
        source.fType = eVK_Unistring;
        source.uValue.fString = makeString(expected.c_str(), expected.size());
        object->properties[keyText()] = source;
        const size_t count = strings.size();
        const size_t before = clears;
        CUTF8String result;
        check(ob_get_a(object, L"k", &result), "existing property");
        C_TEXT text;
        text.setUTF16String(&expected);
        CUTF8String expectedBytes;
        text.copyUTF8String(&expectedBytes);
        check(result == expectedBytes, "exact getter UTF-8 bytes");
        check(strings.size() == count && clears == before + 1, "owning string getter result leak");
        check(textOf(source.uValue.fString) == expected, "source survived getter clear");
        check(!ob_get_b(object, L"k"), "boolean mismatch default");
        check(ob_get_n(object, L"k") == 0, "numeric mismatch default");
        check(!ob_get_c(object, L"k") && !ob_get_o(object, L"k"), "reference mismatch default");
        check(strings.size() == count, "mismatched owning getter result leak");
        release(object);
        balanced("getter final balance");
    }
    for(int kind : {eVK_Boolean, eVK_Real, eVK_Object, eVK_Collection}) {
        Ref *object = newRef();
        PA_Variable value = {};
        if(kind == eVK_Boolean) PA_SetBooleanVariable(&value, true);
        if(kind == eVK_Real) PA_SetRealVariable(&value, 42.5);
        if(kind == eVK_Object) PA_SetObjectVariable(&value, newRef());
        if(kind == eVK_Collection) PA_SetCollectionVariable(&value, newRef());
        object->properties[keyText()] = value;
        size_t before = clears;
        if(kind == eVK_Boolean) check(ob_get_b(object, L"k"), "boolean value");
        if(kind == eVK_Real) check(ob_get_n(object, L"k") == 42.5, "numeric value");
        if(kind == eVK_Boolean || kind == eVK_Real)
            check(clears == before + 1, "scalar result not cleared");
        CUTF8String ignored;
        ob_get_a(object, L"k", &ignored);
        if(kind == eVK_Object || kind == eVK_Collection) {
            ob_get_b(object, L"k");
            ob_get_n(object, L"k");
            if(kind == eVK_Object) check(!ob_get_c(object, L"k"), "collection mismatch");
            else check(!ob_get_o(object, L"k"), "object mismatch");
            void *handoff = kind == eVK_Object ? ob_get_o(object, L"k") : ob_get_c(object, L"k");
            release(object);
            check(refs.count(static_cast<Ref *>(handoff)) && static_cast<Ref *>(handoff)->owners == 1,
                  "owning handoff did not survive source release");
            if(kind == eVK_Object) PA_DisposeObject(handoff);
            else PA_DisposeCollection(handoff);
        }else release(object);
        balanced("mismatched reference result leak");
    }
    Ref *object = newRef();
    object->properties[keyText()] = PA_CreateVariable(eVK_Boolean);
    failGet = true;
    for(int variant = 0; variant < 5; ++variant) {
        size_t before = clears;
        bool caught = false;
        try {
            CUTF8String ignored;
            if(variant == 0) ob_get_a(object, L"k", &ignored);
            if(variant == 1) ob_get_b(object, L"k");
            if(variant == 2) ob_get_n(object, L"k");
            if(variant == 3) ob_get_o(object, L"k");
            if(variant == 4) ob_get_c(object, L"k");
        }catch(const std::runtime_error &error) {
            caught = std::string(error.what()).find("PA_GetObjectProperty failed:") == 0;
        }
        check(caught && clears == before && strings.empty(), "failed getter output inspected/cleared or key leaked");
    }
    failGet = false;
    release(object);
    balanced("failed getter cleanup");
}

static void arrays() {
    PA_Variable array = PA_CreateVariable(eVK_ArrayUnicode);
    PA_ResizeArray(&array, 3);
    const PA_Unichar ascii[] = {'A',0};
    const PA_Unichar bmp[] = {0x65e5,0};
    const PA_Unichar supplementary[] = {0xd83d,0xde00,0};
    const PA_Unichar empty[] = {0};
    const PA_Unichar *fixtures[] = {ascii, bmp, supplementary, empty};
    for(int i = 0; i < 4; ++i) {
        PA_Unistring value = PA_CreateUnistring(const_cast<PA_Unichar *>(fixtures[i]));
        PA_SetStringInArray(array, i, &value);
        check(textOf(PA_GetStringInArray(array, i)) == CUTF16String(fixtures[i]), "accepted independent transfer");
    }
    BytePtr parameters[] = {reinterpret_cast<BytePtr>(&array)};
    ARRAY_TEXT wrapper;
    wrapper.fromParamAtIndex(parameters, 1);
    PA_Unistring borrowed = PA_GetStringInArray(array, 2);
    CUTF16String snapshot = textOf(borrowed);
    PA_Unistring replacement = PA_CreateUnistring(const_cast<PA_Unichar *>(ascii));
    PA_SetStringInArray(array, 2, &replacement);
    CUTF16String copied;
    wrapper.copyUTF16StringAtIndex(&copied, 2);
    check(copied == snapshot, "wrapper failed to copy borrowed getter");
    wrapper.toParamAtIndex(parameters, 1);
    check(textOf(PA_GetStringInArray(array, 2)) == snapshot, "wrapper independent transfer");
    PA_Unistring rejected = PA_CreateUnistring(const_cast<PA_Unichar *>(bmp));
    PA_Variable wrong = PA_CreateVariable(eVK_Longint);
    PA_Variable noData = {};
    noData.fType = eVK_ArrayUnicode;
    PA_SetStringInArray(wrong, 0, &rejected);
    PA_SetStringInArray(noData, 0, &rejected);
    PA_SetStringInArray(array, -1, &rejected);
    PA_SetStringInArray(array, 4, &rejected);
    PA_SetStringInArray(array, 0, nullptr);
    check(textOf(rejected) == CUTF16String(bmp), "rejected transfer consumed input");
    PA_DisposeUnistring(&rejected);
    PA_ResizeArray(&array, 1);
    check(textOf(PA_GetStringInArray(array, 1)) == CUTF16String(bmp), "shrink surviving element");
    PA_ClearVariable(&array);
    balanced("array replacement/shrink/clear");
}

static void stringify() {
    Ref *object = newRef();
    CUTF8String result;
    ob_stringify(object, &result);
    check(result == CUTF8String(reinterpret_cast<const uint8_t *>("{}")), "stringify content");
    release(object);
    balanced("synchronous argument/returned value cleanup");
}

int main(int argc, char **argv) {
    try {
        gCall4D = engine;
        check(argc == 3, "usage: ownership setters|getters|arrays|stringify allocated|null");
        allocatedEmpty = std::string(argv[2]) == "allocated";
        std::string mode(argv[1]);
        if(mode == "setters") setters();
        else if(mode == "getters") getters();
        else if(mode == "arrays") arrays();
        else if(mode == "stringify") stringify();
        else throw std::runtime_error("unknown mode");
        std::printf("PASS %s %s: strings=0 handles=0 refs=0\n", argv[1], argv[2]);
        return 0;
    }catch(const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
