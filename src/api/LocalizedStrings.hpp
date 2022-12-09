#include <fmt/format.h>
#include "../KeyHandler.h"

extern "C" {
#include <glib/gi18n.h>
}

class DummyUserPhraseAdder : public McBopomofo::UserPhraseAdder {
 public:
  void addUserPhrase(const std::string_view& reading,
                     const std::string_view& phrase) {
    std::cerr << fmt::format("Added [{0}]: [{1}]", reading, phrase) << std::endl;
  }
};

class KeyHandlerLocalizedStrings : public McBopomofo::KeyHandler::LocalizedStrings {
 public:
  std::string cursorIsBetweenSyllables(
    const std::string& prevReading,
    const std::string& nextReading) {
    return fmt::format(_("Cursor is between syllables {0} and {1}"),
                       prevReading, nextReading);
  }
  std::string syllablesRequired(size_t syllables) {
    return fmt::format(_("{0} syllables required"), std::to_string(syllables));
  }
  std::string syllablesMaximum(size_t syllables) {
    return fmt::format(_("{0} syllables maximum"), std::to_string(syllables));
  }
  std::string phraseAlreadyExists() {
    return _("phrase already exists");
  }
  std::string pressEnterToAddThePhrase() {
    return _("press Enter to add the phrase");
  }
  std::string markedWithSyllablesAndStatus(
    const std::string& marked,
    const std::string& readingUiText,
    const std::string& status) {
    return fmt::format(_("Marked: {0}, syllables: {1}, {2}"), marked,
                       readingUiText, status);
  }
};
