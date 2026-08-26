#include <windows.h>


//? bypasses problematic comptime c aligned-pointer conversion

static inline HKEY ddcl_hkey_local_machine(void)
{
    return HKEY_LOCAL_MACHINE;
}

static inline HKEY ddcl_hkey_current_user(void)
{
    return HKEY_LOCAL_MACHINE;
}
