#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>

// シンプルなJSONパーサー（glTFメタデータ用）
namespace Json {
    // JSONの値型
    using JsonValue = std::variant<
        std::nullptr_t,
        bool,
        int64_t,
        double,
        std::string,
        std::vector<std::shared_ptr<struct JsonNode>>,
        std::unordered_map<std::string, std::shared_ptr<struct JsonNode>>
    >;

    // JSONノード
    struct JsonNode {
        JsonValue value;
        
        JsonNode() : value(nullptr) {}
        JsonNode(const JsonValue& val) : value(val) {}
        
        // 型チェック
        bool IsNull() const { return std::holds_alternative<std::nullptr_t>(value); }
        bool IsBool() const { return std::holds_alternative<bool>(value); }
        bool IsInt() const { return std::holds_alternative<int64_t>(value); }
        bool IsDouble() const { return std::holds_alternative<double>(value); }
        bool IsString() const { return std::holds_alternative<std::string>(value); }
        bool IsArray() const { return std::holds_alternative<std::vector<std::shared_ptr<JsonNode>>>(value); }
        bool IsObject() const { return std::holds_alternative<std::unordered_map<std::string, std::shared_ptr<JsonNode>>>(value); }
        
        // 値取得
        bool AsBool() const { return std::get<bool>(value); }
        int64_t AsInt() const { return std::get<int64_t>(value); }
        double AsDouble() const { return std::get<double>(value); }
        const std::string& AsString() const { return std::get<std::string>(value); }
        const std::vector<std::shared_ptr<JsonNode>>& AsArray() const { return std::get<std::vector<std::shared_ptr<JsonNode>>>(value); }
        const std::unordered_map<std::string, std::shared_ptr<JsonNode>>& AsObject() const { return std::get<std::unordered_map<std::string, std::shared_ptr<JsonNode>>>(value); }
        
        // 便利メソッド
        std::shared_ptr<JsonNode> Get(const std::string& key) const;
        std::shared_ptr<JsonNode> Get(size_t index) const;
        double AsNumber() const; // IntまたはDoubleを数値として取得
        size_t Size() const;     // ArrayまたはObjectのサイズ
    };

    // JSONパーサークラス
    class Parser {
    public:
        Parser();
        ~Parser();
        
        // JSON文字列を解析
        std::shared_ptr<JsonNode> Parse(const std::string& json);
        
        // エラーメッセージの取得
        const std::string& GetErrorMessage() const { return errorMessage_; }
        
    private:
        std::string errorMessage_;
        std::string jsonText_;
        size_t position_;
        
        // 解析メソッド
        std::shared_ptr<JsonNode> ParseValue();
        std::shared_ptr<JsonNode> ParseObject();
        std::shared_ptr<JsonNode> ParseArray();
        std::shared_ptr<JsonNode> ParseString();
        std::shared_ptr<JsonNode> ParseNumber();
        std::shared_ptr<JsonNode> ParseKeyword(); // true, false, null
        
        // ユーティリティメソッド
        void SkipWhitespace();
        char PeekChar() const;
        char ReadChar();
        bool IsEof() const;
        std::string ReadStringLiteral();
        void SetError(const std::string& message);
        
        // エスケープシーケンス処理
        char ParseEscapeSequence();
        
        // 数値解析
        double ParseNumberValue();
        bool IsDigit(char c) const;
        bool IsHexDigit(char c) const;
    };
}