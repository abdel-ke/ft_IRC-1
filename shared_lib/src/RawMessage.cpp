#include "RawMessage.h"
#include <cctype>

// PARSING

auto IsSpecial(char c) -> bool {
  return (c == '-')
      || (c == '[')
      || (c == ']')
      || (c == '\\')
      || (c == '`')
      || (c == '^')
      || (c == '{')
      || (c == '}');
}

auto IRC::ParseRawMessage(CharStream& s) -> RawMessage {
  RawMessage rawMessage;

  rawMessage.prefix = Maybe<RawPrefix>([](CharStream& s) {
    ParseSymbol(':', s);
    auto prefix = ParsePrefix(s);
    ParseWhitespace(s);
    return prefix;
  }, s);
  rawMessage.command.name = ParseCommandId(s);
  ParseWhitespace(s);
  if (s.Remaining() > 0) {
      rawMessage.command.parameters = ParseParams(s);
  } else {
      rawMessage.command.parameters = std::vector<std::string>{};
  }
  if (s.Remaining() > 0 && s.Peek() == ':') {
    s.Consume();
    rawMessage.command.trailing = std::optional<std::string>{ParseTrailing(s)};
  } else {
    rawMessage.command.trailing = std::nullopt;
  }
  return rawMessage;
}

auto IRC::ParsePrefix(CharStream &s) -> RawPrefix {
  RawPrefix rawPrefix;

  rawPrefix.name = ParseNickname(s);
  rawPrefix.hostname = Maybe<Hostname>([](CharStream& s) {
    ParseSymbol('@', s);
    return ParseHostname(s);
  }, s);
  rawPrefix.username = Maybe<std::string>([](CharStream& s) {
    ParseSymbol('!', s);
    return ParseNickname(s); // TODO: This should probably be *more* lenient
  }, s);
  return rawPrefix;
}

auto IRC::ParseCommandId(CharStream& s) -> std::string {
  try {
    return Attempt<std::string>(ParseWord, s);
  } catch (ParseException& e) {
    return Replicate(ParseDigit, 3, s);
  }
}

auto IRC::ParseParams(CharStream& s) -> std::vector<std::string> {
  std::vector<std::string> params;

  for(;;) {
    ParseWhitespace(s);
    if (s.Peek() == ':') {
      return params;
    } else {
      params.push_back(ParseMiddle(s));
      ParseWhitespace(s);
      if (s.Remaining() == 0) {
        return params;
      }
    }
  }
}

auto PredicateExclude(std::vector<char> blacklist) -> std::function<bool(char)> {
  return [&blacklist](const char c) {
    return std::find(blacklist.begin(), blacklist.end(), c) == blacklist.end();
  };
}

auto IRC::ParseMiddle(CharStream& s) -> std::string {
  std::string accum;

  accum += Satisfy(PredicateExclude({'\n','\r','\0',' ',':'}), s);
  accum += ConsumeWhile(PredicateExclude({'\n','\r','\0',' '}), s);

  return accum;
}

auto IRC::ParseTrailing(CharStream& s) -> std::string {
  return ConsumeWhile(PredicateExclude({'\n','\r','\0'}), s);
}

// TODO: implement this in a better (read: more strict, see RFC 952) way
auto IRC::ParseHostname(CharStream& s) -> Hostname {
  return Hostname(ConsumeWhile1([](char c){
    return std::isalpha(c)
        || c == '-'
        || c == '.'
        || std::isdigit(c);
  }, s));
}

auto IRC::ParseNickname(CharStream& s) -> std::string {
  std::string accum;

  accum += Satisfy([](char c) { return std::isalpha(c); }, s);
  accum += ConsumeWhile([](char c){
    return std::isalpha(c)
        || std::isdigit(c)
        || IsSpecial(c);
  }, s);

  return accum;
}

// COPARSING

auto IRC::operator<<(std::ostream& os, const RawMessage& msg) -> std::ostream& {
    if (msg.prefix.has_value()) {
        os << ":" << *msg.prefix << " ";
    }
    os << msg.command;
        
    return os;
}
auto IRC::operator<<(std::ostream& os, const RawCommand& cmd) -> std::ostream& {
    os << cmd.name 
       << " " 
       << IRC::Coparser::SepBy<std::vector, std::string>(cmd.parameters, " ");
    if (cmd.trailing.has_value())
        os << " :" << *cmd.trailing;
    return os;
}
auto IRC::operator<<(std::ostream& os, const RawPrefix& prefix) -> std::ostream& {
    os << prefix.name;
    if (prefix.hostname.has_value())
        os << "@" << *prefix.hostname;
    if (prefix.username.has_value())
        os << "!" << *prefix.username;
    return os;
}
auto IRC::operator<<(std::ostream& os, const Hostname& hostname) -> std::ostream& {
    os << hostname.value;
    return os;
}
