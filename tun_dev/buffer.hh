#ifndef BUFFER_HH
#define BUFFER_HH

#include <memory>

#include <unistd.h>

class fd_guard {
public:
	fd_guard(int fd) : _fd(fd) { };
	~fd_guard() {
		if (_fd >= 0)
			close(_fd);
	}

	fd_guard(fd_guard&&) = default;
	fd_guard(const fd_guard&) = delete;
	fd_guard& operator=(const fd_guard&) = delete;

	operator int() { return _fd; }

	int disarm() {
		auto f = _fd;
		_fd = -1;
		return f;
	}

private:
	int _fd;
};

class buffer {
public:
	buffer(size_t size) : _data(new char[size]), _size(size) { }
	buffer(char* buf, size_t size) : _data(buf), _size(size) { }

	buffer(buffer&&) = default;
	buffer(const buffer&) = delete;
	buffer& operator=(const buffer&) = delete;

	operator void*() { return _data.get(); }
	operator const void*() const { return _data.get(); }

	void set_size(size_t size) { _size = size; }
	size_t size() const { return _size; }

private:
	std::unique_ptr<char[]> _data;
	size_t _size;
};

class buf_relay {
public:
	buf_relay() : _next(nullptr) { }
	virtual ~buf_relay() { }

	virtual void send(buffer& buf) = 0;

	void set_next(buf_relay& r) {
		_next = &r;
	}

protected:
	void relay_buf(buffer& buf) {
		if (!_next)
			throw std::string { "no next relay connected" };
		_next->send(buf);
	}

private:
	buf_relay* _next;
};

#endif
