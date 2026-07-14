#include <iostream>
#include <regex>
#include <string>
int main() {
  std::string str = "GET /TGUwangqi/login?user=zhangsan&passw=123456 HTTP/1.1";
  std::smatch matches;
  std::regex e(
      "(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?");
  // GET|HEAD|POST|PUT|DELETE 表示匹配并提取其中的任意一个字符串
  //([^?]*) [^?] 表示匹配除?以外的任意字符 *表示0或1个字符串
  //\\?(.*) 表示匹配?字符并提取?字符后面的所有字符直到遇到空格
  // HTTP/1\\.[01] 表示匹配HTTP/1.0或HTTP/1.1
  bool ret = std::regex_match(str, matches, e);
  if (ret == false) {
    return -1;
  }
  for (auto &s : matches) {
    std::cout << s << std::endl;
  }
  return 0;
}