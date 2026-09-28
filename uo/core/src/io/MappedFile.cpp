// SPDX-License-Identifier: BSD-2-Clause
#include "uo/io/MappedFile.h"

#include <utility>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#else
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

namespace uo::io
{

MappedFile::MappedFile(const std::string& path)
{
    open(path);
}

MappedFile::~MappedFile()
{
    close();
}

MappedFile::MappedFile(MappedFile&& other) noexcept
{
    *this = std::move(other);
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept
{
    if (this != &other)
    {
        close();
        _path = std::move(other._path);
        _data = std::exchange(other._data, nullptr);
        _size = std::exchange(other._size, 0);
        _open = std::exchange(other._open, false);
#if defined(_WIN32)
        _file    = std::exchange(other._file, nullptr);
        _mapping = std::exchange(other._mapping, nullptr);
#endif
    }
    return *this;
}

bool MappedFile::open(const std::string& path)
{
    close();
    _path = path;

#if defined(_WIN32)
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen > 0 ? wlen - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

    HANDLE file = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size{};
    GetFileSizeEx(file, &size);
    _file = file;
    _size = static_cast<std::size_t>(size.QuadPart);
    _open = true;

    if (_size == 0)
        return true;

    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping)
    {
        close();
        return false;
    }
    _mapping = mapping;
    _data    = static_cast<const std::uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    if (!_data)
    {
        close();
        return false;
    }
    return true;
#else
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;

    struct stat st{};
    if (fstat(fd, &st) != 0)
    {
        ::close(fd);
        return false;
    }

    _size = static_cast<std::size_t>(st.st_size);
    _open = true;

    if (_size > 0)
    {
        void* p = mmap(nullptr, _size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (p == MAP_FAILED)
        {
            ::close(fd);
            _size = 0;
            _open = false;
            return false;
        }
        _data = static_cast<const std::uint8_t*>(p);
    }

    // The mapping keeps the file alive; the descriptor is no longer needed.
    ::close(fd);
    return true;
#endif
}

void MappedFile::close()
{
#if defined(_WIN32)
    if (_data)
        UnmapViewOfFile(_data);
    if (_mapping)
        CloseHandle(static_cast<HANDLE>(_mapping));
    if (_file)
        CloseHandle(static_cast<HANDLE>(_file));
    _mapping = nullptr;
    _file    = nullptr;
#else
    if (_data)
        munmap(const_cast<std::uint8_t*>(_data), _size);
#endif
    _data = nullptr;
    _size = 0;
    _open = false;
}

std::span<const std::uint8_t> MappedFile::slice(std::size_t offset, std::size_t length) const
{
    if (!_data || offset >= _size)
        return {};
    if (length > _size - offset)
        length = _size - offset;
    return {_data + offset, length};
}

}  // namespace uo::io
