#include <iostream>
#include <regex>
#include <string>

int main() {
  std::string str = "/numbers/1234";

  std::regex e(
      "/numbers/(\\d+)"); //匹配以/numbers/开头的数字，并且在比配过程中提取这个匹配到的字符串
  std::smatch matchs;
  bool ret = std::regex_match(str, matchs, e);
  if (ret == false) {
    return -1;
  }
  for (auto &s : matchs) {
    std::cout << s << std::endl;
  }
  return 0;
}