#include "JsonParser.h"
#include <sstream>
#include <cctype>
#include <cmath>

namespace Json {
    // JsonNode実装
    std::shared_ptr<JsonNode> JsonNode::Get(const std::string& key) const {
        if (IsObject()) {
            const auto& obj = AsObject();
            auto it = obj.find(key);
            if (it != obj.end()) {
                return it->second;
            }
        }
        return nullptr;
    }

    std::shared_ptr<JsonNode> JsonNode::Get(size_t index) const {
        if (IsArray()) {
            const auto& arr = AsArray();
            if (index < arr.size()) {
                return arr[index];
            }
        }
        return nullptr;
    }

    double JsonNode::AsNumber() const {
        if (IsInt()) {
            return static_cast<double>(AsInt());
        } else if (IsDouble()) {
            return AsDouble();
        }
        return 0.0;
    }

    size_t JsonNode::Size() const {
        if (IsArray()) {
            return AsArray().size();
        } else if (IsObject()) {
            return AsObject().size();
        }
        return 0;
    }

    // Parser実装
    Parser::Parser() : position_(0) {}

    Parser::~Parser() {}

    std::shared_ptr<JsonNode> Parser::Parse(const std::string& json) {
        jsonText_ = json;
        position_ = 0;
        errorMessage_.clear();

        SkipWhitespace();
        auto result = ParseValue();
        
        if (!result) {
            return nullptr;
        }
        
        SkipWhitespace();
        if (!IsEof()) {
            SetError("Unexpected characters after JSON value");
            return nullptr;
        }
        
        return result;
    }

    std::shared_ptr<JsonNode> Parser::ParseValue() {
        SkipWhitespace();
        
        if (IsEof()) {
            SetError("Unexpected end of JSON input");
            return nullptr;
        }
        
        char c = PeekChar();
        
        switch (c) {
            case '{':
                return ParseObject();
            case '[':
                return ParseArray();
            case '"':
                return ParseString();
            case 't':
            case 'f':
            case 'n':
                return ParseKeyword();
            case '-':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return ParseNumber();
            default:
                SetError("Unexpected character: " + std::string(1, c));
                return nullptr;
        }
    }

    std::shared_ptr<JsonNode> Parser::ParseObject() {
        std::unordered_map<std::string, std::shared_ptr<JsonNode>> obj;
        
        ReadChar(); // '{'を読み飛ばす
        SkipWhitespace();
        
        if (PeekChar() == '}') {
            ReadChar(); // '}'を読み飛ばす
            return std::make_shared<JsonNode>(obj);
        }
        
        while (true) {
            // キーを読み込む
            SkipWhitespace();
            auto keyNode = ParseString();
            if (!keyNode) {
                return nullptr;
            }
            
            std::string key = keyNode->AsString();
            
            // ':'を読み込む
            SkipWhitespace();
            if (PeekChar() != ':') {
                SetError("Expected ':' after object key");
                return nullptr;
            }
            ReadChar();
            
            // 値を読み込む
            auto valueNode = ParseValue();
            if (!valueNode) {
                return nullptr;
            }
            
            obj[key] = valueNode;
            
            SkipWhitespace();
            char c = PeekChar();
            
            if (c == '}') {
                ReadChar();
                break;
            } else if (c == ',') {
                ReadChar();
                continue;
            } else {
                SetError("Expected ',' or '}' in object");
                return nullptr;
            }
        }
        
        return std::make_shared<JsonNode>(obj);
    }

    std::shared_ptr<JsonNode> Parser::ParseArray() {
        std::vector<std::shared_ptr<JsonNode>> arr;
        
        ReadChar(); // '['を読み飛ばす
        SkipWhitespace();
        
        if (PeekChar() == ']') {
            ReadChar(); // ']'を読み飛ばす
            return std::make_shared<JsonNode>(arr);
        }
        
        while (true) {
            auto valueNode = ParseValue();
            if (!valueNode) {
                return nullptr;
            }
            
            arr.push_back(valueNode);
            
            SkipWhitespace();
            char c = PeekChar();
            
            if (c == ']') {
                ReadChar();
                break;
            } else if (c == ',') {
                ReadChar();
                continue;
            } else {
                SetError("Expected ',' or ']' in array");
                return nullptr;
            }
        }
        
        return std::make_shared<JsonNode>(arr);
    }

    std::shared_ptr<JsonNode> Parser::ParseString() {
        std::string str = ReadStringLiteral();
        if (errorMessage_.empty()) {
            return std::make_shared<JsonNode>(str);
        }
        return nullptr;
    }

    std::shared_ptr<JsonNode> Parser::ParseNumber() {
        double num = ParseNumberValue();
        if (errorMessage_.empty()) {
            // 整数かどうかチェック
            if (std::floor(num) == num && num >= INT64_MIN && num <= INT64_MAX) {
                return std::make_shared<JsonNode>(static_cast<int64_t>(num));
            } else {
                return std::make_shared<JsonNode>(num);
            }
        }
        return nullptr;
    }

    std::shared_ptr<JsonNode> Parser::ParseKeyword() {
        if (jsonText_.substr(position_, 4) == "true") {
            position_ += 4;
            return std::make_shared<JsonNode>(true);
        } else if (jsonText_.substr(position_, 5) == "false") {
            position_ += 5;
            return std::make_shared<JsonNode>(false);
        } else if (jsonText_.substr(position_, 4) == "null") {
            position_ += 4;
            return std::make_shared<JsonNode>(nullptr);
        } else {
            SetError("Invalid keyword");
            return nullptr;
        }
    }

    void Parser::SkipWhitespace() {
        while (position_ < jsonText_.length() && 
               std::isspace(jsonText_[position_])) {
            position_++;
        }
    }

    char Parser::PeekChar() const {
        if (position_ < jsonText_.length()) {
            return jsonText_[position_];
        }
        return '\0';
    }

    char Parser::ReadChar() {
        if (position_ < jsonText_.length()) {
            return jsonText_[position_++];
        }
        return '\0';
    }

    bool Parser::IsEof() const {
        return position_ >= jsonText_.length();
    }

    std::string Parser::ReadStringLiteral() {
        std::string result;
        
        ReadChar(); // '"'を読み飛ばす
        
        while (!IsEof() && PeekChar() != '"') {
            char c = ReadChar();
            
            if (c == '\\') {
                result += ParseEscapeSequence();
            } else {
                result += c;
            }
        }
        
        if (IsEof()) {
            SetError("Unterminated string literal");
            return "";
        }
        
        ReadChar(); // '"'を読み飛ばす
        return result;
    }

    char Parser::ParseEscapeSequence() {
        char c = ReadChar();
        
        switch (c) {
            case '"': return '"';
            case '\\': return '\\';
            case '/': return '/';
            case 'b': return '\b';
            case 'f': return '\f';
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case 'u':
                // Unicode エスケープシーケンス（簡易実装）
                // 実際のUnicode処理は省略
                return '?';
            default:
                SetError("Invalid escape sequence");
                return '\0';
        }
    }

    double Parser::ParseNumberValue() {
        std::string numStr;
        
        // 負号
        if (PeekChar() == '-') {
            numStr += ReadChar();
        }
        
        // 整数部分
        if (PeekChar() == '0') {
            numStr += ReadChar();
        } else if (IsDigit(PeekChar())) {
            while (IsDigit(PeekChar())) {
                numStr += ReadChar();
            }
        } else {
            SetError("Invalid number format");
            return 0.0;
        }
        
        // 小数部分
        if (PeekChar() == '.') {
            numStr += ReadChar();
            if (!IsDigit(PeekChar())) {
                SetError("Invalid number format");
                return 0.0;
            }
            while (IsDigit(PeekChar())) {
                numStr += ReadChar();
            }
        }
        
        // 指数部分
        if (PeekChar() == 'e' || PeekChar() == 'E') {
            numStr += ReadChar();
            if (PeekChar() == '+' || PeekChar() == '-') {
                numStr += ReadChar();
            }
            if (!IsDigit(PeekChar())) {
                SetError("Invalid number format");
                return 0.0;
            }
            while (IsDigit(PeekChar())) {
                numStr += ReadChar();
            }
        }
        
        return std::stod(numStr);
    }

    bool Parser::IsDigit(char c) const {
        return c >= '0' && c <= '9';
    }

    bool Parser::IsHexDigit(char c) const {
        return IsDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }

    void Parser::SetError(const std::string& message) {
        std::stringstream ss;
        ss << "JSON Parse Error at position " << position_ << ": " << message;
        errorMessage_ = ss.str();
    }
}