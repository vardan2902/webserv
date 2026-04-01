#pragma once

class IListener {
public:
	virtual ~IListener() {}

	virtual int fd() = 0;
	virtual int accept() = 0;
	virtual int close() = 0;
};
