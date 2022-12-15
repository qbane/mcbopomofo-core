#include "../LanguageModelLoader.h"

class UserPhraseAdderProxy : public McBopomofo::UserPhraseAdder {
 public:
  UserPhraseAdderProxy(McbpmfApiCore* core) { core_ = core; }
  void addUserPhrase(const std::string_view& reading,
                     const std::string_view& phrase);
 private:
  McbpmfApiCore* core_;
};
