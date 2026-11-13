#ifndef memo_h
#define memo_h
#include <Arduino.h>
#include <vector>

/* Pense-bete : evenements (date, heure facultative, repetition annuelle, texte court) saisis depuis
   la page web /memo.html et stockes dans /memo.csv (flash SPIFFS : efface par uploadfs, la page propose
   une sauvegarde texte). Les 3 prochains sont affiches a la place de la meteo (ligne defilante). */
#define MEMO_MAX        60     // nombre maximal d'evenements
#define MEMO_TEXT_LEN   56     // longueur maximale du texte (octets, accents compris)
#define MEMO_SHOWN      3      // evenements affiches sur l'ecran

struct MemoItem {
  uint16_t y;                  // annee (pour une repetition annuelle : annee de depart)
  uint8_t  m, d;
  int8_t   hh, mm;             // -1 = pas d'heure
  bool     annual;
  char     txt[MEMO_TEXT_LEN];
};

class Memo {
  public:
    bool   setFromCsv(const String& csv);   // remplace toute la liste (valide, trie, sauvegarde)
    String toCsv();
    String toJson();                        // liste + etat + apercu de ce qui s'affiche
    void   buildText(char* out, size_t n);  // texte des 3 prochains evenements (ASCII, "~" entre deux)
    void   reload() { _loaded = false; }    // relit /memo.csv a la prochaine utilisation (apres une restauration)
    bool   changed();                       // le texte affiche a change (date ou heure passee) depuis le dernier buildText
  private:
    bool _loaded = false;
    std::vector<MemoItem> _items;
    String _last;
    void   _ensure();
    bool   _parse(const String& csv);
    bool   _save();
    String _build();
};

extern Memo memo;
#endif
