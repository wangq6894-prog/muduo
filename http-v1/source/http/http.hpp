#include "../server.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <sys/stat.h>
std::unordered_map<int, std::string> _statu_msg = {
    {100, "Continue"},
    {101, "Switching Protocol"},
    {102, "Processing"},
    {103, "Early Hints"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choice"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "unused"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Payload Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {425, "Too Early"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {451, "Unavailable For Legal Reasons"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"}};
std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac", "audio/aac"},
    {".abw", "application/x-abiword"},
    {".arc", "application/x-freearc"},
    {".avi", "video/x-msvideo"},
    {".azw", "application/vnd.amazon.ebook"},
    {".bin", "application/octet-stream"},
    {".bmp", "image/bmp"},
    {".bz", "application/x-bzip"},
    {".bz2", "application/x-bzip2"},
    {".csh", "application/x-csh"},
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".doc", "application/msword"},
    {".docx",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot", "application/vnd.ms-fontobject"},
    {".epub", "application/epub+zip"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".ico", "image/vnd.microsoft.icon"},
    {".ics", "text/calendar"},
    {".jar", "application/java-archive"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".jsonld", "application/ld+json"},
    {".mid", "audio/midi"},
    {".midi", "audio/x-midi"},
    {".mjs", "text/javascript"},
    {".mp3", "audio/mpeg"},
    {".mpeg", "video/mpeg"},
    {".mpkg", "application/vnd.apple.installer+xml"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".oga", "audio/ogg"},
    {".ogv", "video/ogg"},
    {".ogx", "application/ogg"},
    {".otf", "font/otf"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/"
              "vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar", "application/x-rar-compressed"},
    {".rtf", "application/rtf"},
    {".sh", "application/x-sh"},
    {".svg", "image/svg+xml"},
    {".swf", "application/x-shockwave-flash"},
    {".tar", "application/x-tar"},
    {".tif", "image/tiff"},
    {".tiff", "image/tiff"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".vsd", "application/vnd.visio"},
    {".wav", "audio/wav"},
    {".weba", "audio/webm"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml", "application/xml"},
    {".xul", "application/vnd.mozilla.xul+xml"},
    {".zip", "application/zip"},
    {".3gp", "video/3gpp"},
    {".3g2", "video/3gpp2"},
    {".7z", "application/x-7z-compressed"}};
class Util {
public:
  static size_t Split(const std::string &src, const std::string &sep,
                      std::vector<std::string> *arry) {
    size_t offset = 0;
    // 有10个字符，offset是查找的起始位置，范围应该是0~9，offset==10就代表已经越界了
    while (offset < src.size()) {
      size_t pos = src.find(
          sep,
          offset); //在src字符串偏移量offset处，开始向后查找sep字符/字串，返回查找到的位置
      if (pos == std::string::npos) { //没有找到特定的字符
        //将剩余的部分当作一个字串，放入arry中
        if (pos == src.size())
          break;
        arry->push_back(src.substr(offset));
        return arry->size();
      }
      if (pos == offset) {
        offset = pos + sep.size();
        continue; //当前字串是一个空的，没有内容
      }
      arry->push_back(src.substr(offset, pos - offset));
      offset = pos + sep.size();
    }
    return arry->size();
  };
  static bool ReadFile(const std::string &filename, std::string *buf) {
    std::ifstream ifs(filename, std::ios::binary);
    if (ifs.is_open() == false) {
      ERR_LOG("OPEN %s FILE FAILED!!", filename.c_str());
      return false;
    }
    size_t fsize = 0;
    ifs.seekg(0, ifs.end); //跳转读写位置到末尾
    fsize =
        ifs.tellg(); //获取当前读写位置相对于起始位置的偏移量，从末尾偏移刚好就是文件大小
    ifs.seekg(0, ifs.beg); //跳转到起始位置
    buf->resize(fsize);    //开辟文件大小的空间
    ifs.read(&(*buf)[0], fsize);
    if (ifs.good() == false) {
      ERR_LOG("READ %s FILE FAILED!!", filename.c_str());
      ifs.close();
      return false;
    }
    ifs.close();
    return true;
  };
  static bool WriteFile(const std::string &filename, const std::string &buf) {
    std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
    if (ofs.is_open() == false) {
      ERR_LOG("OPEN %s FILE FAILED!!", filename.c_str());
      return false;
    }
    ofs.write(buf.c_str(), buf.size());
    if (ofs.good() == false) {
      ERR_LOG("WRITE %s FILE FAILED!", filename.c_str());
      ofs.close();
      return false;
    }
    ofs.close();
    return true;
  };
  //编码
  static std::string UrlEncode(const std::string url,
                               bool convert_space_to_plus) {
    std::string res;
    for (auto &c : url) {
      //.-_~不被编码，字母或数字直接保留不编码                  is alphanumeric
      if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c) == true) {
        res += c;
        continue;
      }
      if (c == ' ' && convert_space_to_plus == true) {
        res += '+';
        continue;
      }
      char tmp[4];
      // snprintf将字符串格式化成对应的格式并放到对应的空间中
      snprintf(tmp, 4, "%%%02X", c);
      res += tmp;
    }
    return res;
  };
  static char HEXTOI(char c) {
    if (c >= '0' && c <= '9') {
      return c - '0';
    } else if (c >= 'a' && c <= 'z') {
      return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'Z') {
      return c - 'A' + 10;
    }
    return -1;
  }
  //解码
  static std::string UrlDecode(const std::string url,
                               bool convert_plus_to_space) {
    //遇到了%，则将紧随其后的2个字符，转换为数字，第一个数字左移4位，然后加上第二个数字
    //+ -> 2b  %2b->2 << 4 + 11
    std::string res;
    for (int i = 0; i < url.size(); i++) {
      //如果convert_plus_to_space == true，考虑将+转换为空格
      if (url[i] == '+' && convert_plus_to_space == true) {
        res += ' ';
        continue;
      }
      if (url[i] == '%' && (i + 2) < url.size()) {
        char v1 = HEXTOI(url[i + 1]);
        char v2 = HEXTOI(url[i + 2]);
        char v = v1 * 16 + v2;
        res += v;
        i += 2;
        continue;
      }
      res += url[i];
    }
    return res;
  }
  static std::string StatuDesc(int statu) {
    auto it = _statu_msg.find(statu);
    if (it != _statu_msg.end()) {
      return it->second;
    }
    //_statu_msg.find(statu);
    return "Unknow";
  };
  //根据文件后缀获取mime
  static std::string ExtMime(const std::string &filename) {

    // a.b.txt  先获取文件扩展名
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos) {
      return "application/octet-stream";
    }
    //根据扩展名，获取mime
    std::string ext = filename.substr(pos);
    auto it = _mime_msg.find(ext);
    if (it == _mime_msg.end()) {
      return "application/octet-stream";
    }
    return it->second;
  }
  //判断一个文件是否是一个目录
  static bool IsDirectory(const std::string &filename) {
    struct stat st;
    int ret = stat(filename.c_str(), &st);
    if (ret < 0) {
      return false;
    }
    return S_ISDIR(st.st_mode);
  }
  //判断一个文件是否是一个普通文件
  static bool IsRegular(const std::string &filename) {
    struct stat st;
    int ret = stat(filename.c_str(), &st);
    if (ret < 0) {
      return false;
    }
    return S_ISREG(st.st_mode);
  }
  static bool ValidPath(const std::string &path) {
    //思想：按照/进行路径分割，根据有多少子目录，计算目录深度，有多少层，深度不能小于0
    std::vector<std::string> subdir;
    Split(path, "/", &subdir);
    int level = 0;
    for (auto &dir : subdir) {
      if (dir == "..") {
        level--; //任意一层走出相对根目录，就认为有问题
        if (level < 0)
          return false;
        continue;
      }
      level++;
    }
    return true;
  }
};
class HttpRequest {
public:
  std::string _method;
  std::string _path;
  std::string _version;
  std::string _body;
  std::smatch _matches;
  std::unordered_map<std::string, std::string> _headers;
  std::unordered_map<std::string, std::string> _params;

public:
  void Reset() {
    _method.clear();
    _path.clear();
    _version.clear();
    _body.clear();
    std::smatch match;
    _matches.swap(match); //和一个刚创建的新的交换
    _headers.clear();
    _params.clear();
  }
  //插入头部字段
  void SetHeader(std::string &key, std::string &value) {
    _headers.insert(std::make_pair(key, value));
  };
  //判断是否存在指定头部文字
  bool HasHeader(const std::string &key) {
    if (_headers.find(key) == _headers.end()) {
      return false;
    }
    return true;
  };
  //获取指定头部文字的值
  std::string GetHeader(const std::string &key) {
    if (HasHeader(key) == false) {
      return "";
    }
    return _headers[key];
  };
  //插入参数字段
  void SetParam(std::string &key, std::string &value) {
    _params.insert(std::make_pair(key, value));
  };
  //判断是否存在指定参数文字
  bool HasParam(std::string &key) {
    if (_params.find(key) == _params.end()) {
      return false;
    }
    return true;
  };
  //获取指定参数文字的值
  std::string GetParam(std::string &key) {
    if (HasParam(key) == false) {
      return "";
    }
    return _params[key];
  };
  //获取正文长度
  size_t ContentLength() {
    bool ret = HasHeader("Content-Length");
    if (ret == false) {
      return 0;
    }
    std::string clen = GetHeader("Content-Length");
    return std::stoi(clen);
  };
  //判断是否为长连接
  bool IsKeepAlive() {
    if (HasHeader("Connection") == true &&
        GetHeader("Connection") == "keep-alive") {
      return true;
    }
    return false;
  };
};
class HttpResponse {
public:
  int _statu;
  bool _redirect_flag;
  std::string _body;
  std::string _redirect_url;
  std::unordered_map<std::string, std::string> _headers;
  HttpResponse(int statu = 200) : _redirect_flag(false), _statu(statu) {}
  void Reset() {
    _statu = 200;
    _redirect_flag = false;
    _body.clear();
    _redirect_url.clear();
    _headers.clear();
  }
  void SetHeader(const std::string &key, const std::string &value) {
    _headers.insert(std::make_pair(key, value));
  };
  bool HasHeader(const std::string &key) {
    if (_headers.find(key) == _headers.end()) {
      return false;
    }
    return true;
  };
  std::string GetHeader(const std::string &key) {
    if (HasHeader(key) == false) {
      return "";
    }
    return _headers[key];
  };
  void SetContent(std::string &body, const std::string &type = "text/html") {
    _body = body;
    SetHeader("Content-Type", type);
  };
  void SetRedirect(std::string &url, int statu = 302) {
    _statu = statu;
    _redirect_url = url;
    _redirect_flag = true;
  };
  bool CloseConnection() {
    if (HasHeader("Connection") == true &&
        GetHeader("Connection") == "keep-alive") {
      return true;
    }
    return false;
  };
};
typedef enum {
  RECV_HTTP_ERROR,
  RECV_HTTP_LINE,
  RECV_HTTP_HEAD,
  RECV_HTTP_BODY,
  RECV_HTTP_OVER
} HttpRecvStatu;
#define MAX_LINE 8192
//上下文模板
class HttpContext {
private:
  int _resp_statu;           //响应码
  HttpRecvStatu _recv_statu; //接收状态
  HttpRequest _request;      //已解得的状态信息

private:
  //接受并解析请求行
  bool RecvHttpLine(std::string str) {
    std::smatch matches;
    std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? "
                 "(HTTP/1\\.[01])(?:\n|\r\n)?",
                 std::regex::icase);
    // GET|HEAD|POST|PUT|DELETE 表示匹配并提取其中的任意一个字符串
    //([^?]*) [^?] 表示匹配除?以外的任意字符 *表示0或1个字符串
    //\\?(.*) 表示匹配?字符并提取?字符后面的所有字符直到遇到空格
    // HTTP/1\\.[01] 表示匹配HTTP/1.0或HTTP/1.1
    bool ret = std::regex_match(str, matches, e);
    if (ret == false) {
      _recv_statu = RECV_HTTP_ERROR;
      _resp_statu = 400;
      return false;
    }
    //请求方法获取
    _request._method = matches[1];
    std::transform(_request._method.begin(), _request._method.end(),
                   _request._method.begin(), ::toupper);
    //资源路径的获取，需进行URL解码操作，但不需要+转空格
    _request._path = Util::UrlDecode(matches[2], false);
    //协议版本的获取
    _request._version = matches[4];
    //查询字符串的获取
    std::vector<std::string> query_string_arry;
    std::string query_string = matches[3];
    //查询字符串的格式并解析
    Util::Split(query_string, "&", &query_string_arry);
    for (auto &s : query_string_arry) {
      size_t pos = s.find("=");
      if (pos == std::string::npos) {
        _recv_statu = RECV_HTTP_ERROR;
        _resp_statu = 400;
        return false;
      }
      std::string key = Util::UrlDecode(s.substr(0, pos), false);
      std::string value = Util::UrlDecode(s.substr(pos + 1), true);
      _request.SetParam(key, value);
    }
    //首行处理完毕，进入头部获取阶段
    _recv_statu = RECV_HTTP_HEAD;
    return true;
  };
  //读取原始行
  bool ParseHttpLine(Buffer *buf) {
    std::string line = buf->GetLine();
    if (line.size() == 0) {
      //缓冲区中数据不足一行，此时需判断缓冲区中可读数据长度，如果很长却不足一行，就认为是错误URI
      // TOO LONG
      if (buf->ReadAbleSize() > MAX_LINE) {
        _recv_statu = RECV_HTTP_ERROR;
        _resp_statu = 414;
        return false;
      }
      return true;
    }
    if (line.size() > MAX_LINE) {
      _recv_statu = RECV_HTTP_ERROR;
      _resp_statu = 414;
      return false;
    }
    buf->MoveReadOffset(line.size());
    return true;
  };
  //解析请求头
  bool RecvHttpHead(Buffer *buf) {
    while (1) {
      std::string line = buf->GetLineAndPop();
      if (line.size() == 0) {
        if (buf->ReadAbleSize() > MAX_LINE) {
          _recv_statu = RECV_HTTP_ERROR;
          _resp_statu = 414;
          return false;
        }
        return true;
      }
      if (line == "\n" || line == "\r\n") {
        break;
      }
      bool ret = ParseHttpHead(line);
      if (ret == false) {
        return false;
      }
    }
    return true;
  };
  //解析单行请求头
  bool ParseHttpHead(const std::string &line) {
    size_t pos = line.find(": ");
    if (pos == std::string::npos) {
      _recv_statu = RECV_HTTP_ERROR;
      _resp_statu = 400;
      return false;
    }
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 2);
    _request.SetHeader(key, value);
    _recv_statu = RECV_HTTP_BODY;
    //头部处理完毕，进入请求体获取阶段
    return true;
  };
  //解析请求体
  bool RecvHttpBody(Buffer *buf) {
    if (_recv_statu != RECV_HTTP_BODY) {
      return false;
    }
    // 1.获取正文长度
    size_t content_length = _request.ContentLength();
    if (content_length == 0) {
      //无正文，直接返回
      _recv_statu = RECV_HTTP_OVER;
      return true;
    }
    // 2.当前已经接收了多少正文,其实就是缓冲区中已读数据的长度
    size_t real_len = content_length - _request._body.size();
    // 3.接收正文放到body中，并判断当前缓冲区数据中的数据，是否是全部的正文
    if (buf->ReadAbleSize() >= real_len) {
      //缓冲区中数据足够，直接取出数据
      _request._body.append(buf->ReadPosition(), real_len);
      buf->MoveReadOffset(real_len);
      _recv_statu = RECV_HTTP_OVER;
      return true;
    }
    //  如果缓冲区中数据，包含了当前请求的所有正文，则去除所需的数据
    _request._body.append(buf->ReadAsStringAndPop(real_len));
    //  如果无法满足，数据不足，取出数据然后等待数据
    _request._body.append(buf->ReadPosition(), buf->ReadAbleSize());
    buf->MoveReadOffset(buf->ReadAbleSize());
    return true;
  };

public:
  void Reset() {
    _resp_statu = 200;
    _recv_statu = RECV_HTTP_LINE;
    _request.Reset();
    _request._body.clear();
  }
  HttpContext() : _resp_statu(200), _recv_statu(RECV_HTTP_LINE) {}
  int ResponseStatu() { return _resp_statu; };       //返回响应码
  HttpRecvStatu RecvStatu() { return _recv_statu; }; //返回接收状态
  HttpRequest &Request() { return _request; };       //返回请求
  void RecvHttpRequest(Buffer *buf) {                //接受解析请求
    switch (_recv_statu) {
    case RECV_HTTP_LINE:
      RecvHttpLine(buf->GetLineAndPop());
    case RECV_HTTP_HEAD:
      RecvHttpHead(buf);
    case RECV_HTTP_BODY:
      RecvHttpBody(buf);
    case RECV_HTTP_OVER:
      break;
    default:
      break;
    }
  };
};
class HttpServer {
private:
  using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;
  using Handlers = std::vector<std::pair<std::regex, Handler>>;
  Handlers _get_route;
  Handlers _post_route;
  Handlers _put_route;
  Handlers _delete_route;
  std::string _basedir; //静态资源根目录
  TcpServer _server;

private:
  void ErrorHandler(const HttpRequest &req, HttpResponse *rsp) {
    //组织错误展示页面
    std::string body;
    body += "<html>";
    body += "<head>";
    body += "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>";
    body += "</head>";
    body += "<body>";
    body += "<h1>";
    body += std::to_string(rsp->_statu);
    body += " ";
    body += Util::StatuDesc(rsp->_statu);
    body += "</h1>";
    body += "</body>";
    body += "</html>";
    //将页面数据当作响应正文，放入rsp中
    rsp->SetContent(body, "text/html");
  };
  void WriteResponse(const PtrConnection &conn, HttpRequest &req,
                     HttpResponse *rsp) {
    // 1.先完善头部字段
    if (req.IsKeepAlive() == true) {
      rsp->SetHeader("Connection", "keep-alive");
    } else {
      rsp->SetHeader("Connection", "close");
    }
    if (rsp->_body.empty() == false &&
        rsp->HasHeader("Content-Length") == false) {
      rsp->SetHeader("Content-Length", std::to_string(rsp->_body.size()));
    }
    if (rsp->_body.empty() == false &&
        rsp->HasHeader("Content-Type") == false) {
      rsp->SetHeader("Content-Type", "application/octet-stream");
    }
    if (rsp->_redirect_flag == true) {
      rsp->SetHeader("Location", rsp->_redirect_url);
    }
    // 2.将rsp中的要素，按照http协议格式进行组织
    std::string rsp_str;
    rsp_str += req._version + " " + std::to_string(rsp->_statu) + " " +
               Util::StatuDesc(rsp->_statu) + "\r\n";
    for (auto &head : rsp->_headers) {
      rsp_str += head.first + ": " + head.second + "\r\n";
    }
    rsp_str += "\r\n";
    rsp_str += rsp->_body;
    // 3.发送响应
    conn->Send(rsp_str.c_str(), rsp_str.size());
  };
  //静态资源请求处理

  bool IsFileHandler(HttpRequest &req) {
    // 1.必须设置了静态资源根目录，才能判断是否是静态资源请求
    if (_basedir.empty() == true) {
      return false;
    }
    // 2.请求方法，必须是GET / HEAD请求方法
    if (req._method != "GET" && req._method != "HEAD") {
      return false;
    }
    // 3.请求的资源路径必须合法
    if (Util::ValidPath(req._path) == false) {
      return false;
    }
    // 4.请求的资源路径必须存在
    //如果请求的是根目录，这种情况给都变默认最佳一个index.html
    std::string req_path = _basedir + req._path;
    if (req._path.back() == '/') {
      req_path += "/index.html";
    }
    // index.html /image/a.png
    //前缀的相对根目录 /image/a.png -> ./wwwroot/image/a.png
    if (Util::IsRegular(req_path) == false) {
      return false;
    }
    req._path = req_path; //如果请求就是静态资源请求，则有可能需要追加index.html
    return true;
  };
  bool FileHandler(HttpRequest &req, HttpResponse *rsp){
    bool ret = Util::ReadFile(req._path,&rsp->_body);
    if(ret == false){
      rsp->_statu = 404;
      return false;
    }
    std::string mime = Util::ExtMime(req._path);
    rsp->SetHeader("Content-Type", mime);
    rsp->_statu = 200;
    return true;
  };
  void Dispatcher(HttpRequest &req, HttpResponse *rsp,
                  Handlers &handlers) {
    //在对应请求方法的路由表中，查找是否含有对应资源请求的处理函数，又则调用，没有404
    //路由表中存贮的键值对为：正则表达式-处理函数
    //使用正则表达式，对请求的资源路径进行正则匹配，匹配成功就使用对应函数进行处理
    // /number/(\d+) 匹配 /number/123
    for (auto &handler : handlers) {
      std::regex &re = handler.first;
      Handler &functor = handler.second;
      bool ret = std::regex_match(req._path, req._matches, re);
      if (ret == false) {
        continue;
      }
      return functor(req, rsp);
    }
    rsp->_statu = 404;
  };
  void Route(HttpRequest &req, HttpResponse *rsp) {
    //对请求进行分辨，是静态资源请求还是功能性请求
    // 静态资源请求，则进行静态资源的处理
    // 功能性请求，则需要通过几个请求路由表来确定是否有处理函数
    // 既不是静态资源请求，也不是功能性请求，则回复404响应
    if (IsFileHandler(req) == true) {
      //静态资源请求
      FileHandler(req, rsp);
      return;
    }
    //功能性请求
    if (req._method == "GET" || req._method == "HEAD") {
      return Dispatcher(req, rsp, _get_route);
    } else if (req._method == "POST") {
      return Dispatcher(req, rsp, _post_route);
    } else if (req._method == "PUT") {
      return Dispatcher(req, rsp, _put_route);
    } else if (req._method == "DELETE") {
      return Dispatcher(req, rsp, _delete_route);
    }
    //到这里说明都没匹配成功
    rsp->_statu = 405; // 405 Method Not Allowed
  };
  //设置上下文
  void OnConnected(const PtrConnection &conn) {
    // 1.创建上下文
    conn->SetContext(new HttpContext());
    DBG_LOG("NEW CONNECTION %p", conn.get());
  };
  //核心函数
  void OnMessage(const PtrConnection &conn, Buffer *buffer) {
    // 1.获取上下文
    HttpContext *context = conn->GetContext()->get<HttpContext>();
    // 2.通过上下文对缓冲区数据进行解析，得到HttpResponse对象
    //如果缓冲区的数据解析出错，就直接回复出错响应
    //如果解析正常且请求已获取完毕，才开始去进行处理
    context->RecvHttpRequest(buffer);
    HttpRequest &req = context->Request();
    HttpResponse rsp;
    rsp._statu = context->ResponseStatu();
    if (context->ResponseStatu() >= 400) {
      //解析出错，直接回复出错响应
      ErrorHandler(req, &rsp);
      WriteResponse(PtrConnection(conn), req, &rsp);
      return;
    }
    if (context->RecvStatu() != RECV_HTTP_OVER) {
      //不是完整请求
      return;
      //退出，等待新数据到来重新继续处理
    }
    // 3.请求路由 业务处理
    Route(req, &rsp);
    // 4.对HttpResponse对象进行组织并发送
    WriteResponse(PtrConnection(conn), req, &rsp);
    // 5.重置上下文
    context->Reset();
    // 6.根据是否为长连接，判断是否需要关闭连接
    if (rsp.CloseConnection() == true)
      conn->Shutdown();
  };

public:
  HttpServer(int port,int timeout = DEFAULT_TIMEOUT):_server(port){
    _server.EnableInactiveRelease(timeout);
    _server.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
    _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
  }
  void SetBaseDir(const std::string &path){
    _basedir = path;
  };
  void Get(const std::string &pattern, Handler handler){
    _get_route.push_back(std::make_pair(std::regex(pattern), handler));
  };
  void Post(const std::string &pattern, Handler handler){
    _post_route.push_back(std::make_pair(std::regex(pattern), handler));
  };
  void Put(const std::string &pattern, Handler handler){
    _put_route.push_back(std::make_pair(std::regex(pattern), handler));
  };
  void Delete(const std::string &pattern, Handler handler){
    _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
  };
  void SetThreadCount(int count){
    _server.SetThreadCount(count);
  };
  void Listen(){
    _server.Start();
  };
};
