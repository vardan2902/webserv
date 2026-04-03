#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#include "IResponseManager.hpp"

class ResponseManager : public IResponseManager {
	struct MultipartPart {
		std::string filename;
		std::string body;
	};

public:
	ResponseManager();
	ResponseManager(const ResponseManager&);
	ResponseManager& operator=(const ResponseManager&);
	~ResponseManager();

	std::string build(const HttpRequest& req, const Server& server, const Location* location) const;

private:
	HttpResponse  _collect(const HttpRequest& req, const Server& server, const Location* location) const;
	std::string   build_raw(const HttpResponse& response) const;
	std::string   _statusMessage(int statusCode) const;

	// Pre-checks
	HttpResponse  _handleRedirect(const Location* location) const;

	// Method handlers (uniform signature for dispatch map)
	HttpResponse  _handleDelete(const HttpRequest&, const Location*, const std::string& filePath, const std::string& index) const;
	HttpResponse  _handlePost  (const HttpRequest&, const Location*, const std::string& filePath, const std::string& index) const;
	HttpResponse  _handleGet   (const HttpRequest&, const Location*, const std::string& filePath, const std::string& index) const;

	// GET sub-handlers
	HttpResponse  _serveDirectory(const HttpRequest&, const Location*, const std::string& filePath, const std::string& index) const;
	HttpResponse  _serveFile     (const std::string& filePath) const;

	// POST sub-handlers
	HttpResponse  _handleMultipart(const std::string& body, const std::string& boundary, const std::string& uploadStore) const;
	bool          _splitParts(const std::string& body, const std::string& boundary, std::vector<MultipartPart>& out) const;
	bool          _parsePart(const std::string& raw, MultipartPart& out) const;
	bool          _writeFile(const std::string& dest, const std::string& data) const;
};
