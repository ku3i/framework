/* basic.cpp */

#include "basic.h"
#include <algorithm>

FILE*
open_file(const char* mode, const char* format, ...)
{
    char filename[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(filename, 1024, format, args);
    va_end(args);

    FILE * fd = fopen(filename, mode);
    if (NULL == fd)
    {
        perror("Error");
        err_msg(__FILE__, __LINE__, "could not open file %s in mode %s.", filename, mode);
    }

    return fd;
}

namespace basic {

std::string
make_directory(const char *format, ...)
{
    char foldername[256];
    va_list args;
    va_start(args, format);
    vsnprintf(foldername, 256, format, args);
    va_end(args);

    int md = mkdir(foldername, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (!md) sts_msg("created folder %s", foldername);
    else if (errno != EEXIST) err_msg(__FILE__, __LINE__, "could not create folder %s.\n %s\n", foldername, strerror(errno));

    return foldername;
}

Filelist list_directory(const char* target_dir, const char* filter)
{
    const unsigned max_files = 1024;
    std::vector<std::string> files_in_directory;
    struct dirent *epdf;
    DIR *dpdf;

    if (0 == strcmp(target_dir,""))
        dpdf = opendir("./");
    else
        dpdf = opendir(target_dir);

    if (dpdf != NULL) {
        while ((epdf = readdir(dpdf)) and (files_in_directory.size() < max_files)) {
            if (strcmp(epdf->d_name, ".") and strcmp(epdf->d_name, "..") and (nullptr != strstr(epdf->d_name, filter)))
                files_in_directory.emplace_back(epdf->d_name);
        }
        if (files_in_directory.size() > 1)
            std::sort(files_in_directory.begin(), files_in_directory.end());
        sts_msg("Read %u files in directory %s", files_in_directory.size(), target_dir);
        for (std::size_t i = 0; i < std::min(std::size_t{10}, files_in_directory.size()); ++i)
            sts_msg("\t%s", files_in_directory[i].c_str());
        if (files_in_directory.size()>10)
            sts_msg("\t...truncated.");
    } else err_msg(__FILE__, __LINE__, "Could not open directory %s", target_dir);

    return files_in_directory;
}

std::size_t get_file_size(FILE* fd)
{
    // obtain file size
    if (fd == nullptr) return 0;
    fseek(fd, 0, SEEK_END);
    long int file_size = ftell(fd);
    rewind(fd);
    return (file_size > 0) ? file_size : 0;
}

std::string get_timestamp(void) {
    time_t t0 = time(NULL);                 // initialize time
    struct tm * timeinfo = localtime(&t0);  // get time info
    char timestamp[256];
    snprintf(
        timestamp, 256, "%02d%02d%02d%02d%02d%02d",
        timeinfo->tm_year-100, timeinfo->tm_mon + 1,
        timeinfo->tm_mday, timeinfo->tm_hour,
        timeinfo->tm_min, timeinfo->tm_sec
        );
    return timestamp;
}

} /* namespace basic */
