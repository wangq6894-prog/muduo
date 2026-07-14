#include <iostream>
#include <functional>

// 普通函数
int add(int a, int b) {
    return a + b;
}

// 成员函数
class Calculator {
public:
    int multiply(int a, int b) {
        return a * b;
    }
    
    void greet(const std::string& name) {
        std::cout << "Hello, " << name << "!" << std::endl;
    }
};

int main() {
    // ============================================
    // 1. 绑定普通函数 — 提前绑定部分参数
    // ============================================
    auto add5 = std::bind(add, std::placeholders::_1, 5);
    std::cout << "add5(10) = " << add5(10) << std::endl;  // 10 + 5 = 15

    // ============================================
    // 2. 占位符 _1, _2 ... 表示调用时才传入的参数
    // ============================================
    // _1 对应第一个实参，_2 对应第二个实参
    auto add2 = std::bind(add, std::placeholders::_2, std::placeholders::_1);
    std::cout << "add2(3, 7) = " << add2(3, 7) << std::endl;  // 7 + 3 = 10

    // ============================================
    // 3. 绑定成员函数 — 需要传入对象指针/引用
    // ============================================
    Calculator calc;

    // 绑定成员函数: &类::函数, 对象指针, 占位符...
    auto mul = std::bind(&Calculator::multiply, &calc, 
                          std::placeholders::_1, std::placeholders::_2);
    std::cout << "calc.multiply(6, 7) = " << mul(6, 7) << std::endl;  // 42

    // ============================================
    // 4. 绑定成员函数 — 固定参数
    // ============================================
    auto double_it = std::bind(&Calculator::multiply, &calc, 
                                std::placeholders::_1, 2);
    std::cout << "double_it(10) = " << double_it(10) << std::endl;  // 20

    // ============================================
    // 5. 绑定 void 成员函数
    // ============================================
    auto greet = std::bind(&Calculator::greet, &calc, 
                            std::placeholders::_1);
    greet("World");  // Hello, World!

    // ============================================
    // 6. 绑定到 lambda
    // ============================================
    auto lambda = [](int x, int y) { return x - y; };
    auto sub5 = std::bind(lambda, std::placeholders::_1, 5);
    std::cout << "sub5(20) = " << sub5(20) << std::endl;  // 20 - 5 = 15

    return 0;
}
