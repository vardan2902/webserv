#include "ResponseManager.hpp"

ResponseManager::ResponseManager() {}
ResponseManager::ResponseManager(const ResponseManager&) {}
ResponseManager& ResponseManager::operator=(const ResponseManager&) { return *this; }
ResponseManager::~ResponseManager() {}

std::string ResponseManager::_statusMessage(int statusCode) const {
	switch (statusCode) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		default:  return "Unknown";
	}
}

// ─── helpers ─────────────────────────────────────────────────────────────────

static HttpResponse _makeResponse(int code, const std::string& body = "") {
	HttpResponse r;
	r.statusCode = code;
	r.body = body;
	return r;
}

static bool _isMethodAllowed(const std::string& method, const std::vector<std::string>& allowed) {
	for (size_t i = 0; i < allowed.size(); ++i)
		if (allowed[i] == method) return true;
	return false;
}

// ─── pre-checks ──────────────────────────────────────────────────────────────

HttpResponse ResponseManager::_handleRedirect(const Location* location) const {
	HttpResponse response = _makeResponse(location->returnCode);
	response.headers["Location"] = location->returnUrl;
	return response;
}

// ─── method handlers ─────────────────────────────────────────────────────────

HttpResponse ResponseManager::_handleDelete(
	const HttpRequest& req, const Location*, const std::string& filePath, const std::string&
) const {
	(void)req;
	if (access(filePath.c_str(), F_OK) != 0)
		return _makeResponse(404, "404 Not Found");
	unlink(filePath.c_str());
	return _makeResponse(204);
}

HttpResponse ResponseManager::_handlePost(
	const HttpRequest& req, const Location* location,
	const std::string&, const std::string&
) const {
	if (!location || location->uploadStore.empty())
		return _makeResponse(501, "501 Not Implemented");

	std::map<std::string, std::string>::const_iterator ctIt =
		req.headers.find("Content-Type");
	if (ctIt == req.headers.end())
		return _makeResponse(400, "400 Bad Request");

	const std::string& ct = ctIt->second;
	std::string boundaryPrefix = "boundary=";
	size_t bpos = ct.find(boundaryPrefix);
	if (ct.find("multipart/form-data") == std::string::npos || bpos == std::string::npos)
		return _makeResponse(400, "400 Bad Request");

	std::string boundary = ct.substr(bpos + boundaryPrefix.size());
	size_t semi = boundary.find(';');
	if (semi != std::string::npos)
		boundary = boundary.substr(0, semi);
	while (!boundary.empty() &&
	       (boundary[boundary.size()-1] == '\r' || boundary[boundary.size()-1] == ' '))
		boundary.erase(boundary.size()-1);

	return _handleMultipart(req.body, boundary, location->uploadStore);
}

// ─── POST sub-handlers ───────────────────────────────────────────────────────

bool ResponseManager::_parsePart(const std::string& raw, MultipartPart& out) const {
	size_t sep = raw.find("\r\n\r\n");
	if (sep == std::string::npos) return false;

	std::string headers = raw.substr(0, sep);

	size_t cdpos = headers.find("Content-Disposition:");
	if (cdpos == std::string::npos) return false;

	size_t cdend = headers.find("\r\n", cdpos);
	std::string cdLine = headers.substr(
		cdpos, cdend == std::string::npos ? std::string::npos : cdend - cdpos);

	const std::string filenameKey = "filename=\"";
	size_t fnpos = cdLine.find(filenameKey);
	if (fnpos == std::string::npos) return false;
	fnpos += filenameKey.size();
	size_t fnend = cdLine.find('"', fnpos);
	if (fnend == std::string::npos) return false;

	std::string filename = cdLine.substr(fnpos, fnend - fnpos);
	size_t sl = filename.rfind('/');
	if (sl != std::string::npos) filename = filename.substr(sl + 1);
	size_t bs = filename.rfind('\\');
	if (bs != std::string::npos) filename = filename.substr(bs + 1);
	if (filename.empty()) return false;

	out.filename = filename;
	out.body     = raw.substr(sep + 4);
	return true;
}

bool ResponseManager::_splitParts(
	const std::string& body,
	const std::string& boundary,
	std::vector<MultipartPart>& out
) const {
	std::string delim      = "\r\n--" + boundary;
	std::string firstDelim = "--" + boundary;

	size_t start = body.find(firstDelim);
	if (start == std::string::npos) return false;
	start += firstDelim.size();
	if (start + 2 <= body.size() && body[start] == '\r' && body[start+1] == '\n')
		start += 2;

	while (true) {
		size_t dpos = body.find(delim, start);
		if (dpos == std::string::npos) break;

		MultipartPart part;
		if (_parsePart(body.substr(start, dpos - start), part))
			out.push_back(part);

		size_t afterDelim = dpos + delim.size();
		if (afterDelim + 2 <= body.size() &&
		    body[afterDelim] == '-' && body[afterDelim+1] == '-')
			break;
		if (afterDelim + 2 <= body.size() &&
		    body[afterDelim] == '\r' && body[afterDelim+1] == '\n')
			afterDelim += 2;
		start = afterDelim;
	}
	return !out.empty();
}

bool ResponseManager::_writeFile(const std::string& dest, const std::string& data) const {
	int fd = open(dest.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) return false;

	const char* ptr     = data.c_str();
	size_t      total   = data.size();
	size_t      written = 0;
	while (written < total) {
		ssize_t n = write(fd, ptr + written, total - written);
		if (n < 0) { close(fd); return false; }
		written += static_cast<size_t>(n);
	}
	close(fd);
	return true;
}

HttpResponse ResponseManager::_handleMultipart(
	const std::string& body,
	const std::string& boundary,
	const std::string& uploadStore
) const {
	std::vector<MultipartPart> parts;
	if (!_splitParts(body, boundary, parts))
		return _makeResponse(400, "400 Bad Request");

	std::string lastFilename;
	for (size_t i = 0; i < parts.size(); ++i) {
		std::string dest = uploadStore;
		if (dest[dest.size()-1] != '/') dest += '/';
		dest += parts[i].filename;
		if (!_writeFile(dest, parts[i].body))
			return _makeResponse(500, "500 Internal Server Error");
		lastFilename = parts[i].filename;
	}

	HttpResponse resp = _makeResponse(201);
	resp.headers["Location"] = uploadStore + "/" + lastFilename;
	resp.body = "<html><body>"
	            "<h2>Upload successful: " + lastFilename + "</h2>"
	            "<p><a href=\"/files/\">View uploaded files</a></p>"
	            "</body></html>";
	return resp;
}

HttpResponse ResponseManager::_handleGet(
	const HttpRequest& req, const Location* location, const std::string& filePath, const std::string& index
) const {
	if (!filePath.empty() && filePath[filePath.size() - 1] == '/')
		return _serveDirectory(req, location, filePath, index);
	return _serveFile(filePath);
}

// ─── GET sub-handlers ────────────────────────────────────────────────────────

HttpResponse ResponseManager::_serveDirectory(
	const HttpRequest& req, const Location* location, const std::string& filePath, const std::string& index
) const {
	std::ifstream indexFile((filePath + index).c_str());
	if (indexFile.is_open()) {
		std::ostringstream ss;
		ss << indexFile.rdbuf();
		return _makeResponse(200, ss.str());
	}

	if (location && location->autoindex) {
		DIR* dir = opendir(filePath.c_str());
		if (!dir)
			return _makeResponse(403, "403 Forbidden");
		std::ostringstream html;
		html << "<html><head><title>Index of " << req.path << "</title></head>"
		     << "<body><h1>Index of " << req.path << "</h1><ul>";
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL)
			html << "<li><a href=\"" << entry->d_name << "\">" << entry->d_name << "</a></li>";
		closedir(dir);
		html << "</ul></body></html>";
		return _makeResponse(200, html.str());
	}

	return _makeResponse(403, "403 Forbidden");
}

HttpResponse ResponseManager::_serveFile(const std::string& filePath) const {
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
		return _makeResponse(404, "404 Not Found");
	std::ostringstream ss;
	ss << file.rdbuf();
	return _makeResponse(200, ss.str());
}

// ─── collect ─────────────────────────────────────────────────────────────────

HttpResponse ResponseManager::_collect(const HttpRequest& req, const Server& server, const Location* location) const {
	if (location && location->returnCode != 0)
		return _handleRedirect(location);

	if (location && !location->allowMethods.empty() && !_isMethodAllowed(req.method, location->allowMethods))
		return _makeResponse(405, "405 Method Not Allowed");

	std::string root     = location && !location->root.empty()  ? location->root  : server.root;
	std::string index    = location && !location->index.empty() ? location->index : "index.html";
	std::string filePath = root + req.path;

	typedef HttpResponse (ResponseManager::*MethodHandler)(
		const HttpRequest&, const Location*, const std::string&, const std::string&
	) const;
	typedef std::map<std::string, MethodHandler> MethodHandlerMap;

	static MethodHandlerMap handlers;
	if (handlers.empty()) {
		handlers["DELETE"] = &ResponseManager::_handleDelete;
		handlers["POST"]   = &ResponseManager::_handlePost;
		handlers["GET"]    = &ResponseManager::_handleGet;
	}

	MethodHandlerMap::const_iterator it = handlers.find(req.method);
	if (it == handlers.end())
		return _makeResponse(405, "405 Method Not Allowed");
	return (this->*it->second)(req, location, filePath, index);
}

// ─── build ───────────────────────────────────────────────────────────────────

std::string ResponseManager::build_raw(const HttpResponse& response) const {
	std::ostringstream oss;
	oss << "HTTP/1.1 " << response.statusCode << " " << _statusMessage(response.statusCode) << "\r\n"
	    << "Content-Length: " << response.body.size() << "\r\n"
	    << "Content-Type: text/html\r\n";
	for (std::map<std::string, std::string>::const_iterator it = response.headers.begin(); it != response.headers.end(); ++it)
		oss << it->first << ": " << it->second << "\r\n";
	oss << "\r\n" << response.body;
	return oss.str();
}

std::string ResponseManager::build(const HttpRequest& req, const Server& server, const Location* location) const {
	HttpResponse response = _collect(req, server, location);

	if (response.statusCode >= 400 && response.statusCode <= 599) {
		std::map<int, std::string>::const_iterator it = server.errorPages.find(response.statusCode);
		if (it != server.errorPages.end()) {
			std::ifstream errFile((server.root + it->second).c_str());
			if (errFile.is_open()) {
				std::ostringstream ss;
				ss << errFile.rdbuf();
				response.body = ss.str();
			}
		}
	}

	return build_raw(response);
}
