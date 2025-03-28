#ifndef API_MANAGER_H
#define API_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// Returns the number of practices found (or -1 on error)
int api_manager_check_practices(const char* url, const char* token);

#ifdef __cplusplus
}
#endif

#endif // API_MANAGER_H
