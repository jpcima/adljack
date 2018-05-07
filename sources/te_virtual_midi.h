//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <windows.h>

enum {
    TE_VM_LOGGING_MISC = 1,
    TE_VM_LOGGING_RX = 2,
    TE_VM_LOGGING_TX = 4,
};

enum {
    TE_VM_FLAGS_PARSE_RX = 1,
    TE_VM_FLAGS_PARSE_TX = 2,
    TE_VM_FLAGS_INSTANTIATE_RX_ONLY = 4,
    TE_VM_FLAGS_INSTANTIATE_TX_ONLY = 8,
    TE_VM_FLAGS_INSTANTIATE_BOTH = 12,
};

typedef struct _VM_MIDI_PORT VM_MIDI_PORT, *LPVM_MIDI_PORT;
typedef void (CALLBACK *LPVM_MIDI_DATA_CB)(LPVM_MIDI_PORT, LPBYTE, DWORD, DWORD_PTR);

#define VIRTUALMIDI_DYNAMIC_FUNCTIONS(F)                                \
    F(CreatePortEx2, LPVM_MIDI_PORT, LPCWSTR, LPVM_MIDI_DATA_CB, DWORD_PTR, DWORD, DWORD) \
    F(CreatePortEx3, LPVM_MIDI_PORT, LPCWSTR, LPVM_MIDI_DATA_CB, DWORD_PTR, DWORD, DWORD, GUID *, GUID *) \
    F(ClosePort, void, LPVM_MIDI_PORT)                                  \
    F(SendData, BOOL, LPVM_MIDI_PORT, LPBYTE, DWORD)                    \
    F(GetData, BOOL, LPVM_MIDI_PORT, LPBYTE, PDWORD)                    \
    F(GetProcesses, BOOL, LPVM_MIDI_PORT, ULONG64 *, PDWORD)            \
    F(Shutdown, BOOL, LPVM_MIDI_PORT)                                   \
    F(GetVersion, LPCWSTR, PWORD, PWORD, PWORD, PWORD)                  \
    F(GetDriverVersion, LPCWSTR, PWORD, PWORD, PWORD, PWORD)            \
    F(Logging, DWORD, DWORD)

template <class = void>
struct virtualMIDI_ODR {
public:
    #define VIRTUALMIDI_DYNAMIC_DECLARE_LPFN(x, type, ...)  \
        static type (CALLBACK *x)(__VA_ARGS__);
    VIRTUALMIDI_DYNAMIC_FUNCTIONS(VIRTUALMIDI_DYNAMIC_DECLARE_LPFN)

    static bool load();
    static void unload();

private:
    static HMODULE handle_;
};

typedef virtualMIDI_ODR<> virtualMIDI;

#define VIRTUALMIDI_DYNAMIC_DEFINE_LPFN(x, type, ...)                   \
    template <class T> type (CALLBACK *virtualMIDI_ODR<T>::x)(__VA_ARGS__) = nullptr;
VIRTUALMIDI_DYNAMIC_FUNCTIONS(VIRTUALMIDI_DYNAMIC_DEFINE_LPFN)

template <class T> HMODULE virtualMIDI_ODR<T>::handle_ = nullptr;


template <class T>
bool virtualMIDI_ODR<T>::load()
{
    HMODULE handle = nullptr;

    if (!handle && sizeof(void *) == 4)
        handle = LoadLibraryA("teVirtualMIDI32.dll");
    if (!handle && sizeof(void *) == 8)
        handle = LoadLibraryA("teVirtualMIDI64.dll");
    if (!handle)
        handle = LoadLibraryA("teVirtualMIDI.dll");
    if (!handle)
        return false;

    handle_ = handle;

    #define VIRTUALMIDI_LOAD_FUNCTION(x, type, ...)                 \
        virtualMIDI_ODR<T>::x = (type (CALLBACK *)(__VA_ARGS__))    \
            GetProcAddress(handle, "virtualMIDI" #x);               \
        if (!virtualMIDI_ODR<T>::x) { unload(); return false; }
    VIRTUALMIDI_DYNAMIC_FUNCTIONS(VIRTUALMIDI_LOAD_FUNCTION);

    return true;
}

template <class T>
void virtualMIDI_ODR<T>::unload()
{
    HMODULE handle = handle_;
    if (!handle)
        return;

    FreeLibrary(handle);
    handle_ = nullptr;

    #define VIRTUALMIDI_UNLOAD_FUNCTION(x, ...)     \
        virtualMIDI_ODR<T>::x = nullptr;
    VIRTUALMIDI_DYNAMIC_FUNCTIONS(VIRTUALMIDI_UNLOAD_FUNCTION);
}
