#include "Xml.h"

#include <cstdlib>

namespace nt::xml
{

const Node* Node::child (const std::string& n) const
{
    for (const auto& c : children)
        if (c.name == n) return &c;
    return nullptr;
}

std::vector<const Node*> Node::all (const std::string& n) const
{
    std::vector<const Node*> out;
    for (const auto& c : children)
        if (c.name == n) out.push_back (&c);
    return out;
}

std::string Node::attr (const std::string& n, const std::string& fallback) const
{
    for (const auto& [k, v] : attributes)
        if (k == n) return v;
    return fallback;
}

std::string Node::childText (const std::string& n, const std::string& fallback) const
{
    const auto* c = child (n);
    return c != nullptr ? c->text : fallback;
}

double Node::childNumber (const std::string& n, double fallback) const
{
    const auto* c = child (n);
    if (c == nullptr) return fallback;
    const char* s = c->text.c_str();
    char* end = nullptr;
    const double v = std::strtod (s, &end);
    return end == s ? fallback : v;
}

namespace
{
void appendUtf8 (std::string& out, unsigned long code)
{
    if (code < 0x80) out += static_cast<char> (code);
    else if (code < 0x800)
    {
        out += static_cast<char> (0xc0 | (code >> 6));
        out += static_cast<char> (0x80 | (code & 0x3f));
    }
    else if (code < 0x10000)
    {
        out += static_cast<char> (0xe0 | (code >> 12));
        out += static_cast<char> (0x80 | ((code >> 6) & 0x3f));
        out += static_cast<char> (0x80 | (code & 0x3f));
    }
    else
    {
        out += static_cast<char> (0xf0 | (code >> 18));
        out += static_cast<char> (0x80 | ((code >> 12) & 0x3f));
        out += static_cast<char> (0x80 | ((code >> 6) & 0x3f));
        out += static_cast<char> (0x80 | (code & 0x3f));
    }
}

std::string decode (const std::string& s)
{
    std::string out;
    out.reserve (s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] != '&') { out += s[i]; continue; }
        const auto semi = s.find (';', i);
        if (semi == std::string::npos) { out += s[i]; continue; }
        const std::string e = s.substr (i + 1, semi - i - 1);
        if (e == "amp") out += '&';
        else if (e == "lt") out += '<';
        else if (e == "gt") out += '>';
        else if (e == "quot") out += '"';
        else if (e == "apos") out += '\'';
        else if (! e.empty() && e[0] == '#')
            appendUtf8 (out, (e.size() > 1 && (e[1] == 'x' || e[1] == 'X')) ? std::strtoul (e.c_str() + 2, nullptr, 16)
                                                                             : std::strtoul (e.c_str() + 1, nullptr, 10));
        else { out += s.substr (i, semi - i + 1); }
        i = semi;
    }
    return out;
}

std::string trim (const std::string& s)
{
    const auto a = s.find_first_not_of (" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of (" \t\r\n");
    return s.substr (a, b - a + 1);
}

struct Parser
{
    const std::string& t;
    size_t p = 0;
    std::string error;

    bool startsWith (const char* s) const { return t.compare (p, std::char_traits<char>::length (s), s) == 0; }
    void skipSpace() { while (p < t.size() && (t[p] == ' ' || t[p] == '\t' || t[p] == '\r' || t[p] == '\n')) ++p; }

    // Comments, processing instructions and the DOCTYPE, wherever they are.
    bool skipMarkup()
    {
        if (startsWith ("<!--"))
        {
            const auto e = t.find ("-->", p);
            p = e == std::string::npos ? t.size() : e + 3;
            return true;
        }
        if (startsWith ("<?"))
        {
            const auto e = t.find ("?>", p);
            p = e == std::string::npos ? t.size() : e + 2;
            return true;
        }
        if (startsWith ("<!DOCTYPE") || startsWith ("<!doctype"))
        {
            // A DOCTYPE can hold an internal subset in brackets.
            int depth = 0;
            for (; p < t.size(); ++p)
            {
                if (t[p] == '[') ++depth;
                else if (t[p] == ']') --depth;
                else if (t[p] == '>' && depth <= 0) { ++p; break; }
            }
            return true;
        }
        return false;
    }

    std::string name()
    {
        const size_t a = p;
        while (p < t.size() && ! (t[p] == ' ' || t[p] == '\t' || t[p] == '\r' || t[p] == '\n' || t[p] == '>' || t[p] == '/' || t[p] == '='))
            ++p;
        return t.substr (a, p - a);
    }

    bool element (Node& node, int depth)
    {
        if (depth > 200) { error = "nested too deeply"; return false; }
        if (p >= t.size() || t[p] != '<') { error = "expected an element"; return false; }
        ++p;
        node.name = name();
        if (node.name.empty()) { error = "an element with no name"; return false; }
        for (;;)
        {
            skipSpace();
            if (p >= t.size()) { error = "the file ends inside <" + node.name + ">"; return false; }
            if (t[p] == '/')
            {
                if (p + 1 < t.size() && t[p + 1] == '>') { p += 2; return true; }
                error = "a stray / in <" + node.name + ">";
                return false;
            }
            if (t[p] == '>') { ++p; break; }
            std::string key = name();
            skipSpace();
            if (p >= t.size() || t[p] != '=') { error = "an attribute with no value in <" + node.name + ">"; return false; }
            ++p;
            skipSpace();
            if (p >= t.size() || (t[p] != '"' && t[p] != '\'')) { error = "an unquoted attribute in <" + node.name + ">"; return false; }
            const char q = t[p++];
            const auto e = t.find (q, p);
            if (e == std::string::npos) { error = "an unclosed attribute in <" + node.name + ">"; return false; }
            node.attributes.emplace_back (key, decode (t.substr (p, e - p)));
            p = e + 1;
        }
        // Content.
        std::string text;
        for (;;)
        {
            if (p >= t.size()) { error = "<" + node.name + "> is never closed"; return false; }
            if (t[p] == '<')
            {
                if (startsWith ("</"))
                {
                    p += 2;
                    const auto closing = name();
                    skipSpace();
                    if (p < t.size() && t[p] == '>') ++p;
                    if (closing != node.name) { error = "<" + node.name + "> is closed by </" + closing + ">"; return false; }
                    node.text = trim (text);
                    return true;
                }
                if (startsWith ("<![CDATA["))
                {
                    const auto e = t.find ("]]>", p);
                    if (e == std::string::npos) { error = "an unclosed CDATA section"; return false; }
                    text += t.substr (p + 9, e - p - 9);
                    p = e + 3;
                    continue;
                }
                if (skipMarkup()) continue;
                node.children.emplace_back();
                if (! element (node.children.back(), depth + 1)) return false;
                continue;
            }
            const auto e = t.find ('<', p);
            const auto chunk = t.substr (p, (e == std::string::npos ? t.size() : e) - p);
            text += decode (chunk);
            p = e == std::string::npos ? t.size() : e;
        }
    }
};
} // namespace

ParseResult parse (const std::string& text)
{
    ParseResult res;
    Parser parser { text, 0, {} };
    // A byte order mark, then anything before the root element.
    if (text.compare (0, 3, "\xEF\xBB\xBF") == 0) parser.p = 3;
    for (;;)
    {
        parser.skipSpace();
        if (parser.p >= text.size()) { res.error = "There is no XML in this file."; return res; }
        if (parser.skipMarkup()) continue;
        break;
    }
    auto root = std::make_unique<Node>();
    if (! parser.element (*root, 0))
    {
        res.error = "The XML is damaged: " + parser.error + ".";
        return res;
    }
    res.root = std::move (root);
    return res;
}

//==============================================================================

Writer::Writer (const std::string& prolog) : out (prolog) {}

std::string Writer::escape (const std::string& s)
{
    std::string o;
    for (char c : s)
    {
        switch (c)
        {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    }
    return o;
}

std::string Writer::attributes (const Attributes& attrs)
{
    std::string o;
    for (const auto& [k, v] : attrs) o += " " + k + "=\"" + escape (v) + "\"";
    return o;
}

void Writer::indent() { out.append (stack.size() * 2, ' '); }

void Writer::open (const std::string& name, const Attributes& attrs)
{
    indent();
    out += "<" + name + attributes (attrs) + ">\n";
    stack.push_back (name);
}

void Writer::close()
{
    if (stack.empty()) return;
    const auto name = stack.back();
    stack.pop_back();
    indent();
    out += "</" + name + ">\n";
}

void Writer::leaf (const std::string& name, const std::string& text, const Attributes& attrs)
{
    indent();
    out += "<" + name + attributes (attrs) + ">" + escape (text) + "</" + name + ">\n";
}

void Writer::leaf (const std::string& name, long long number, const Attributes& attrs)
{
    leaf (name, std::to_string (number), attrs);
}

void Writer::empty (const std::string& name, const Attributes& attrs)
{
    indent();
    out += "<" + name + attributes (attrs) + "/>\n";
}

void Writer::raw (const std::string& line) { out += line + "\n"; }

} // namespace nt::xml
