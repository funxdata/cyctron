
// static const char* get_mime_type(const char *path) {
//     const char *ext = strrchr(path, '.');
//     if (!ext) return "application/octet-stream";
//     if (strcmp(ext, ".html") == 0) return "text/html";
//     if (strcmp(ext, ".css") == 0) return "text/css";
//     if (strcmp(ext, ".js") == 0) return "application/javascript";
//     if (strcmp(ext, ".png") == 0) return "image/png";
//     if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
//     if (strcmp(ext, ".gif") == 0) return "image/gif";
//     if (strcmp(ext, ".ico") == 0) return "image/x-icon";
//     if (strcmp(ext, ".json") == 0) return "application/json";
//     return "application/octet-stream";
// }