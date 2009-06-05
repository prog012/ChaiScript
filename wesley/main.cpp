#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <iostream>
#include <map>
#include <fstream>

#include "boxedcpp.hpp"
#include "bootstrap.hpp"
#include "bootstrap_stl.hpp"

#include "langkit_lexer.hpp"
#include "langkit_parser.hpp"

class TokenType { public: enum Type { File, Whitespace, Identifier, Number, Operator, Parens_Open, Parens_Close,
    Square_Open, Square_Close, Curly_Open, Curly_Close, Comma, Quoted_String, Single_Quoted_String, Carriage_Return, Semicolon,
    Function_Def, Scoped_Block, Statement, Equation, Return, Expression, Term, Factor, Negate, Comment,
    Value, Fun_Call, Method_Call, Poetry_Call, Comparison }; };

char *tokentype_to_string(int tokentype) {
    char *token_types[] = {"File", "Whitespace", "Identifier", "Number", "Operator", "Parens_Open", "Parens_Close",
        "Square_Open", "Square_Close", "Curly_Open", "Curly_Close", "Comma", "Quoted_String", "Single_Quoted_String", "Carriage_Return", "Semicolon",
        "Function_Def", "Scoped_Block", "Statement", "Equation", "Return", "Expression", "Term", "Factor", "Negate", "Comment",
        "Value", "Fun_Call", "Method_Call", "Poetry_Call", "Comparison" };

    return token_types[tokentype];
}

struct ParserError {
    std::string reason;

    ParserError(const std::string &why) : reason(why) { }
};

struct EvalError {
    std::string reason;

    EvalError(const std::string &why) : reason(why) { }
};

Boxed_Value eval_token(BoxedCPP_System &ss, TokenPtr node);

void debug_print(TokenPtr token, std::string prepend) {
    std::cout << prepend << "Token: " << token->text << "(" << tokentype_to_string(token->identifier) << ") @ " << token->filename
        << ": ("  << token->start.line << ", " << token->start.column << ") to ("
        << token->end.line << ", " << token->end.column << ") " << std::endl;

    for (unsigned int i = 0; i < token->children.size(); ++i) {
        debug_print(token->children[i], prepend + "  ");
    }
}

void debug_print(std::vector<TokenPtr> &tokens) {
    for (unsigned int i = 0; i < tokens.size(); ++i) {
        debug_print(tokens[i], "");
    }
}

//A function that prints any string passed to it

template <typename T>
void print(const T &t)
{
    std::cout << t << std::endl;
}

template<> void print<bool>(const bool &t)
{
    if (t) {
        std::cout << "true" << std::endl;
    }
    else {
        std::cout << "false" << std::endl;
    }
}

std::string concat_string(const std::string &s1, const std::string &s2) {
    return s1+s2;
}

const Boxed_Value add_two(const Boxed_Value &val1, const Boxed_Value &val2) {
    return Boxed_Value(1);
}

std::string load_file(const char *filename) {
    std::ifstream infile (filename, std::ios::in | std::ios::ate);

    if (!infile.is_open()) {
        std::cerr << "Can not open " << filename << std::endl;
        exit(0);
    }

    std::streampos size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::vector<char> v(size);
    infile.read(&v[0], size);

    std::string ret_val (v.empty() ? std::string() : std::string (v.begin(), v.end()).c_str());

    return ret_val;
}

Boxed_Value eval_function0(BoxedCPP_System &ss, TokenPtr node, std::vector<std::string> &param_names) {
    return eval_token(ss, node);
}
Boxed_Value eval_function1(BoxedCPP_System &ss, TokenPtr node, std::vector<std::string> &param_names, const Boxed_Value &arg0) {
    ss.add_object(param_names[0], arg0);

    return eval_token(ss, node);
}
Boxed_Value eval_function2(BoxedCPP_System &ss, TokenPtr node, std::vector<std::string> &param_names, const Boxed_Value &arg0,
        const Boxed_Value &arg1) {

    ss.add_object(param_names[0], arg0);
    ss.add_object(param_names[1], arg1);

    return eval_token(ss, node);
}
Boxed_Value eval_function3(BoxedCPP_System &ss, TokenPtr node, std::vector<std::string> &param_names, const Boxed_Value &arg0,
        const Boxed_Value &arg1, const Boxed_Value &arg2) {

    ss.add_object(param_names[0], arg0);
    ss.add_object(param_names[1], arg1);
    ss.add_object(param_names[2], arg2);

    return eval_token(ss, node);
}

Boxed_Value eval_token(BoxedCPP_System &ss, TokenPtr node) {
    Boxed_Value retval;
    unsigned int i, j;

    switch (node->identifier) {
        case (TokenType::Value) :
        case (TokenType::File) :
            for (i = 0; i < node->children.size(); ++i) {
                retval = eval_token(ss, node->children[i]);
            }
        break;
        case (TokenType::Identifier) :
            if (node->text == "true") {
                retval = Boxed_Value(true);
            }
            else if (node->text == "false") {
                retval = Boxed_Value(false);
            }
            else {
                retval = ss.get_object(node->text);
            }
        break;
        case (TokenType::Number) :
            retval = Boxed_Value(double(atof(node->text.c_str())));
        break;
        case (TokenType::Quoted_String) :
            retval = Boxed_Value(node->text);
        break;
        case (TokenType::Single_Quoted_String) :
            retval = Boxed_Value(node->text);
        break;
        case (TokenType::Equation) :
            retval = eval_token(ss, node->children.back());
            if (node->children.size() > 1) {
                for (i = node->children.size()-2; ((int)i) >= 0; --i) {
                    ss.add_object(node->children[i]->text, retval);
                }
            }
        break;
        case (TokenType::Factor) :
        case (TokenType::Expression) :
        case (TokenType::Term) :
        case (TokenType::Comparison) : {
            retval = eval_token(ss, node->children[0]);
            if (node->children.size() > 1) {
                for (i = 1; i < node->children.size(); i += 2) {
                    Param_List_Builder plb;
                    plb << retval;
                    plb << eval_token(ss, node->children[i + 1]);

                    try {
                        retval = dispatch(ss.get_function(node->children[i]->text), plb);
                    }
                    catch(std::exception &e){
                        throw EvalError("Can not find appropriate '" + node->children[i]->text + "'");
                    }
                }
            }
        }
        break;
        case (TokenType::Negate) : {
            retval = eval_token(ss, node->children[0]);
            Param_List_Builder plb;
            plb << retval;
            plb << Boxed_Value(-1);

            try {
                retval = dispatch(ss.get_function("*"), plb);
            }
            catch(std::exception &e){
                throw EvalError("Can not find appropriate negation");
            }
        }
        break;

        case (TokenType::Fun_Call) : {
            Param_List_Builder plb;
            for (i = 1; i < node->children.size(); ++i) {
                plb << eval_token(ss, node->children[i]);
            }
            try {
                retval = dispatch(ss.get_function(node->children[0]->text), plb);
            }
            catch(std::exception &e){
                throw EvalError("Can not find appropriate '" + node->children[0]->text + "'");
            }
        }
        break;
        case (TokenType::Method_Call) : {

            retval = eval_token(ss, node->children[0]);
            if (node->children.size() > 1) {
                for (i = 1; i < node->children.size(); ++i) {
                    Param_List_Builder plb;
                    plb << retval;

                    for (j = 1; j < node->children[i]->children.size(); ++j) {
                        plb << eval_token(ss, node->children[i]->children[j]);
                    }

                    try {
                        retval = dispatch(ss.get_function(node->children[i]->children[0]->text), plb);
                    }
                    catch(std::exception &e){
                        throw EvalError("Can not find appropriate '" + node->children[i]->children[0]->text + "'");
                    }
                }
            }
        }
        break;
        case (TokenType::Poetry_Call) : {
            Param_List_Builder plb;

            plb << eval_token(ss, node->children[0]);

            for (i = 2; i < node->children.size(); ++i) {
                plb << eval_token(ss, node->children[i]);
            }
            try {
                retval = dispatch(ss.get_function(node->children[1]->text), plb);
            }
            catch(std::exception &e){
                throw EvalError("Can not find appropriate '" + node->children[1]->text + "'");
            }
        }
        break;
        case (TokenType::Function_Def) : {
            unsigned int num_args = node->children.size() - 2;
            std::vector<std::string> param_names;
            for (i = 0; i < num_args; ++i) {
                param_names.push_back(node->children[i+1]->text);
            }
            switch (num_args) {
                case (0) : {
                    std::cout << "Registered a 0 function" << std::endl;
                    ss.register_function(boost::function<Boxed_Value (void)>
                        (boost::bind(&eval_function0, boost::ref(ss), node->children.back(), param_names)), node->children[0]->text);
                    std::cout << "Reg: " << node->children[0]->text << std::endl;
                }
                break;
                case (1) : {
                    std::cout << "Registered a 1 (" << node->children[1]->text << ") function" << std::endl;
                    ss.register_function(boost::function<Boxed_Value (const Boxed_Value &)>
                        (boost::bind(&eval_function1, boost::ref(ss), node->children.back(), param_names, _1)), node->children[0]->text);
                    std::cout << "Reg: " << node->children[0]->text << std::endl;
                }
                break;
                case (2) : {
                    std::cout << "Registered a 2 (" << node->children[1]->text << "," << node->children[2]->text << ") function" << std::endl;
                    ss.register_function(boost::function<Boxed_Value (const Boxed_Value &, const Boxed_Value &)>
                        (boost::bind(&eval_function2, boost::ref(ss), node->children.back(), param_names, _1, _2)), node->children[0]->text);
                    std::cout << "Reg: " << node->children[0]->text << std::endl;
                }
                break;
                case (3) : {
                    std::cout << "Registered a 3 (" << node->children[1]->text << "," << node->children[2]->text << "," << node->children[3]->text << ") function" << std::endl;
                    ss.register_function(boost::function<Boxed_Value (const Boxed_Value &, const Boxed_Value &, const Boxed_Value &)>
                        (boost::bind(&eval_function3, boost::ref(ss), node->children.back(), param_names, _1, _2, _3)), node->children[0]->text);
                    std::cout << "Reg: " << node->children[0]->text << std::endl;
                }
                break;
            }
        }
        break;
        case (TokenType::Scoped_Block) :
        case (TokenType::Statement) :
        case (TokenType::Return) :
        case (TokenType::Carriage_Return) :
        case (TokenType::Semicolon) :
        case (TokenType::Comment) :
        case (TokenType::Operator) :
        case (TokenType::Whitespace) :
        case (TokenType::Parens_Open) :
        case (TokenType::Parens_Close) :
        case (TokenType::Square_Open) :
        case (TokenType::Square_Close) :
        case (TokenType::Curly_Open) :
        case (TokenType::Curly_Close) :
        case (TokenType::Comma) :
        break;
    }

    return retval;
}

Rule build_parser_rules() {
    Rule params;
    Rule block(TokenType::Scoped_Block);
    Rule fundef(TokenType::Function_Def);
    Rule statement(TokenType::Statement);
    Rule return_statement(TokenType::Return);
    Rule equation(TokenType::Equation);
    Rule comparison(TokenType::Comparison);
    Rule expression(TokenType::Expression);
    Rule term(TokenType::Term);
    Rule factor(TokenType::Factor);
    Rule negate(TokenType::Negate);
    Rule funcall(TokenType::Fun_Call);
    Rule methodcall(TokenType::Method_Call);
    Rule poetrycall(TokenType::Poetry_Call);
    Rule value;
    Rule statements;

    Rule rule = *(fundef | statements);
    statements = (equation >> *(Ign(Id(TokenType::Semicolon)) >> equation) >> *(Ign(Id(TokenType::Semicolon))));
    fundef = Ign(Str("def")) >> Id(TokenType::Identifier) >> ~(Ign(Str("(")) >> ~params >> Ign(Str(")"))) >> block;
    params = Id(TokenType::Identifier) >> *(Ign(Str(",")) >> Id(TokenType::Identifier));
    block = Ign(Str("{")) >> ~statements >> Ign(Str("}"));
    equation = *(Id(TokenType::Identifier) >> Ign(Str("="))) >> comparison;
    comparison = expression >> *((Str("==") >> expression) | (Str("!=") >> expression) | (Str("<") >> expression) |
            (Str("<=") >> expression) |(Str(">") >> expression) | (Str(">=") >> expression));
    expression = term >> *((Str("+") >> term) | (Str("-") >> term));
    term = factor >> *((Str("*") >> factor) | (Str("/") >> factor));
    factor = methodcall | poetrycall | value | negate | (Ign(Str("+")) >> value);
    funcall = Id(TokenType::Identifier) >> Ign(Id(TokenType::Parens_Open)) >> ~(expression >> *(Ign(Str("," )) >> expression)) >> Ign(Id(TokenType::Parens_Close));
    methodcall = value >> +(Ign(Str(".")) >> funcall);
    poetrycall = value >> +(value);
    negate = Ign(Str("-")) >> factor;
    return_statement = Ign(Str("return")) >> expression;

    value = (Ign(Id(TokenType::Parens_Open)) >> expression >> Ign(Id(TokenType::Parens_Close))) | return_statement |
        funcall | Id(TokenType::Identifier) | Id(TokenType::Number) | Id(TokenType::Quoted_String) | Id(TokenType::Single_Quoted_String) ;

    return rule;
}

Lexer build_lexer() {
    Lexer lexer;
    lexer.set_skip(Pattern("[ \\t]+", TokenType::Whitespace));
    lexer.set_line_sep(Pattern("\\n|\\r\\n", TokenType::Carriage_Return));
    lexer.set_command_sep(Pattern(";|\\r\\n|\\n", TokenType::Semicolon));
    lexer.set_multiline_comment(Pattern("/\\*", TokenType::Comment), Pattern("\\*/", TokenType::Comment));
    lexer.set_singleline_comment(Pattern("//", TokenType::Comment));

    lexer << Pattern("[A-Za-z_]+", TokenType::Identifier);
    lexer << Pattern("[0-9]+(\\.[0-9]+)?", TokenType::Number);
    lexer << Pattern("[!@#$%^&*\\-+=<>.]+|/[!@#$%^&\\-+=<>]*", TokenType::Operator);
    lexer << Pattern("\\(", TokenType::Parens_Open);
    lexer << Pattern("\\)", TokenType::Parens_Close);
    lexer << Pattern("\\[", TokenType::Square_Open);
    lexer << Pattern("\\]", TokenType::Square_Close);
    lexer << Pattern("\\{", TokenType::Curly_Open);
    lexer << Pattern("\\}", TokenType::Curly_Close);
    lexer << Pattern(",", TokenType::Comma);
    lexer << Pattern("\"(?:[^\"\\\\]|\\\\.)*\"", TokenType::Quoted_String);
    lexer << Pattern("'(?:[^'\\\\]|\\\\.)*'", TokenType::Single_Quoted_String);

    return lexer;
}

BoxedCPP_System build_eval_system() {
    BoxedCPP_System ss;
    bootstrap(ss);
    bootstrap_vector<std::vector<int> >(ss);
    //dump_system(ss);

    //Register a new function, this one with typing for us, so we don't have to ubox anything
    //right here
    ss.register_function(boost::function<void (const bool &)>(&print<bool>), "print");
    ss.register_function(boost::function<void (const std::string &)>(&print<std::string>), "print");
    ss.register_function(boost::function<void (const double &)>(&print<double>), "print");
    ss.register_function(boost::function<std::string (const std::string &, const std::string &)>(concat_string), "concat_string");
    ss.register_function(boost::function<Boxed_Value (Boxed_Value, Boxed_Value)>(add_two), "add_two");

    return ss;
}

TokenPtr parse(Rule &rule, std::vector<TokenPtr> &tokens, const char *filename) {

    Token_Iterator iter = tokens.begin(), end = tokens.end();
    TokenPtr parent(new Token("Root", TokenType::File, filename));

    std::pair<Token_Iterator, bool> results = rule(iter, end, parent);

    if (results.second) {
        //debug_print(parent, "");
        return parent;
    }
    else {
        throw ParserError("Parse failed");
    }
}

Boxed_Value evaluate_string(Lexer &lexer, Rule &parser, BoxedCPP_System &ss, const std::string &input, const char *filename) {
    std::vector<TokenPtr> tokens = lexer.lex(input, filename);
    Boxed_Value value;

    for (unsigned int i = 0; i < tokens.size(); ++i) {
        if ((tokens[i]->identifier == TokenType::Quoted_String) || (tokens[i]->identifier == TokenType::Single_Quoted_String)) {
            tokens[i]->text = tokens[i]->text.substr(1, tokens[i]->text.size()-2);
        }
    }

    //debug_print(tokens);
    try {
        TokenPtr parent = parse(parser, tokens, "INPUT");
        value = eval_token(ss, parent);
    }
    catch (ParserError &pe) {
        std::cout << "Parsing error: " << pe.reason << std::endl;
    }
    catch (EvalError &ee) {
        std::cout << "Eval error: " << ee.reason << std::endl;
    }
    catch (std::exception &e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    return value;
}

int main(int argc, char *argv[]) {
    std::string input;

    Lexer lexer = build_lexer();
    Rule parser = build_parser_rules();
    BoxedCPP_System ss = build_eval_system();

    if (argc < 2) {
        std::cout << "eval> ";
        std::getline(std::cin, input);
        while (input != "quit") {
            Boxed_Value val = evaluate_string(lexer, parser, ss, input, "INPUT");
            if (*(val.get_type_info().m_bare_type_info) != typeid(void)) {
                std::cout << "result: ";
                dispatch(ss.get_function("print"), Param_List_Builder() << val);
            }
            std::cout << "eval> ";
            std::getline(std::cin, input);
        }
    }
    else {
        for (int i = 1; i < argc; ++i) {
            Boxed_Value val = evaluate_string(lexer, parser, ss, load_file(argv[i]), argv[i]);
        }
    }
}
