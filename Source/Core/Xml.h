/*
    Xml - just enough XML to read and write MusicXML, with no JUCE so the
    MusicXML code stays in the core and is tested with it (decision 0021).

    Reading builds a small tree of elements, attributes and text. Comments,
    processing instructions and the DOCTYPE are skipped; CDATA is text; the
    five named entities and numeric character references are decoded. It is
    not a validating parser and does not need to be: MusicXML is plain.
*/

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nt::xml
{

struct Node
{
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string text;              // the element's own text, entities decoded
    std::vector<Node> children;

    const Node* child (const std::string& n) const;
    std::vector<const Node*> all (const std::string& n) const;
    bool has (const std::string& n) const { return child (n) != nullptr; }
    std::string attr (const std::string& n, const std::string& fallback = {}) const;
    std::string childText (const std::string& n, const std::string& fallback = {}) const;
    double childNumber (const std::string& n, double fallback) const;
};

struct ParseResult
{
    std::unique_ptr<Node> root;
    std::string error;
};

ParseResult parse (const std::string& text);

// Writes indented XML. Elements are opened and closed in order.
class Writer
{
public:
    using Attributes = std::vector<std::pair<std::string, std::string>>;

    explicit Writer (const std::string& prolog = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
    void open (const std::string& name, const Attributes& attrs = {});
    void close();
    void leaf (const std::string& name, const std::string& text, const Attributes& attrs = {});
    void leaf (const std::string& name, long long number, const Attributes& attrs = {});
    void empty (const std::string& name, const Attributes& attrs = {});
    void raw (const std::string& line);
    const std::string& str() const { return out; }

private:
    std::string out;
    std::vector<std::string> stack;
    void indent();
    static std::string escape (const std::string& s);
    static std::string attributes (const Attributes& attrs);
};

} // namespace nt::xml
