#include <iostream>
#include <string>
#include <typeinfo>
#include <unistd.h>

#include <any>
class Any {
private:
  class holder {
  public:
    virtual ~holder() {}
    virtual const std::type_info &type() = 0;
    virtual holder *clone() = 0;
  };
  template <class T> class placeholder : public holder {
  public:
    placeholder(const T &val) : _val(val) {}
    ~placeholder() {}
    virtual const std::type_info &type() {
      return typeid(T);
    } //获取子类对象保存的数据类型
    virtual holder *clone() {
      return new placeholder<T>(_val);
    }; //针对当前的对象自身，克隆出一个新的子类对象
    T _val;
  };
  holder *_content;

public:
  Any() : _content(nullptr) {}
  template <class T> Any(const T &val) : _content(new placeholder<T>(val)){};
  Any(const Any &other)
      : _content(other._content ? other._content->clone() : nullptr) {}
  ~Any() {
    if (_content) {
      delete _content;
      _content = nullptr;
    }
  }
  Any swap(Any &other) {
    std::swap(_content, other._content);
    return *this;
  };

  template <class T> T *get() {
    if (typeid(T) != _content->type()) {
      throw nullptr;
    }
    return &((placeholder<T> *)_content)->_val;
  }; //返回子类对象保存的数据指针
  template <class T> Any &operator=(const T &val) {
    Any(val).swap(*this);
    return *this;
  };
  Any &operator=(const Any &other) {
    Any(other).swap(*this);
    return *this;
  };
};

class Test {
public:
  Test() { std::cout << "构造" << std::endl; }
  Test(const Test &t) { std::cout << "拷贝构造" << std::endl; }
  ~Test() { std::cout << "析构" << std::endl; }
};
int main() {
  std::any a;
  a = 10;
  int *pi = std::any_cast<int>(&a);
  std::cout << *pi << std::endl;
  a = std::string("nihao");
  std::string *ps = std::any_cast<std::string>(&a);
  std::cout << *ps << std::endl;
  // Any a;
  // {
  //   Test t;
  //   a = t;
  // }
  // a = 10;
  // int *pa = a.get<int>();
  // a = std::string("nihao");
  // std::string *ps = a.get<std::string>();
  // std::cout << *ps << std::endl;
  // while(1){
  //   sleep(1);
    
  // }
  // return 0;
}
