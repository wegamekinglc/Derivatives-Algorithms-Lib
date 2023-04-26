//
// Created by wegam on 2023/4/26.
//

#include <string>
#include <vector>
#include <regex>
#include <iostream>

using namespace std;

/*
 * 解析的表达式，由不同的节点构成
 */

struct Node {
    virtual std::string repr() const = 0;
    virtual ~Node() {}
};

struct NodeConst : Node {
    // 常量节点，就是一个float数
    double val_;
    explicit NodeConst(double val): val_(val) {}

    std::string repr() const override {
        return to_string(val_);
    }
};

struct NodeVar : Node {
    // 变量节点，有自己的名字
    string name_;
    double value_;
    explicit NodeVar(const string& name): name_(name), value_(0.0) {}

    std::string repr() const override {
        return name_;
    }

};

struct NodeAssign : Node {
    // 赋值节点，右手值（rhs）赋值给左手变量（lhs)
    std::unique_ptr<Node> lhs_;
    std::unique_ptr<Node> rhs_;
    NodeAssign(std::unique_ptr<Node>& lhs, std::unique_ptr<Node>& rhs): lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

    std::string repr() const override {
        return lhs_->repr() + " = " + rhs_->repr();
    }
};


struct NodePlus : Node {
    // 加法节点，右手值/变量（rhs）与左手值/变量（lhs) 相加
    std::unique_ptr<Node> lhs_;
    std::unique_ptr<Node> rhs_;
    NodePlus(std::unique_ptr<Node>& lhs, std::unique_ptr<Node>& rhs): lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

    std::string repr() const override {
        return lhs_->repr() + " + " + rhs_->repr();
    }
};

/*
 * 解析规则
 */

vector<string> tokenize(const string& str) {
    /*
     * 使用正则表达式，完成对于单行语句的切分
     * e.g: `s = 2 + a` 会被分解为 ['s', '=', '2', 'a']
     */
    static const std::regex r(R"([\w.]+|[\+]|[=])");
    vector<string> v;
    for (std::regex_iterator<string::const_iterator> it(str.begin(), str.end(), r), end; it != end; ++it)
        v.push_back(string((*it)[0]));
    return v;
}


struct Parser {
    using TokIt = typename vector<string>::const_iterator;
    using Statement = std::unique_ptr<Node>;

    static Statement parse(TokIt& cur, const TokIt& end) {
        // 完整语句解析，现在支持赋值语句
        auto lhs = parse_var(cur);
        if (*cur == "=")
            return parse_assign(cur, end, lhs);
    }

    static Statement parse_var(TokIt& cur) {
        auto top = std::make_unique<NodeVar>(*cur);
        ++cur;
        return std::move(top);
    }

    static Statement parse_assign(TokIt& cur, const TokIt& end, Statement& lhs) {
        // 赋值节点解析
        ++cur;
        auto rhs = parse_expr(cur, end);
        return std::make_unique<NodeAssign>(lhs, rhs);
    }

    static Statement parse_expr(TokIt& cur, const TokIt& end) {
        // 表达式解析，现在我们只支持：加法
        auto lhs = parse_var_const(cur, end);
        ++cur;
        while (cur != end && (*cur)[0] == '+') {
            ++cur;
            auto rhs = parse_var_const(cur, end);
            lhs = make_unique<NodePlus>(lhs, rhs);
        }
        return lhs;
    }

    static Statement parse_var_const(TokIt& cur, const TokIt& end) {
        /*
         *  根据字符的特征，选择parse为Const节点还是Var节点；
         *  这里我们简单的假设，凡是以数字开头的都是Const常量
         */
        if ((*cur)[0] == '.' || ((*cur)[0] >= '0' && (*cur)[0] <= '9')) {
            size_t idx;
            return std::make_unique<NodeConst>(std::stod(*cur, &idx));
        }

        // When parse const fails, we have a variable
        return parse_var(cur);
    }

};


int main() {
    string sample = "Var1 = 2.0 + 3.0";
    auto tokens = tokenize(sample);
    Parser::TokIt it = tokens.begin();
    auto root = Parser::parse(it, tokens.end());
    std::cout << root->repr() << std::endl;
    return 0;
}
