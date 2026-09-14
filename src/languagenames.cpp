#include "languagenames.h"

#include <QChar>
#include <QHash>

namespace LanguageNames {

namespace {

struct Entry
{
    const char *code;

    // The ISO 639-1 two-letter code, or empty where the language has none and
    // where the entry is a variant rather than a language - "chi_tra" carries no
    // "zh" because "chi_sim" already has it, and a phone set to Chinese has to
    // resolve to one of them.
    const char *iso1;

    const char *endonym;
    const char *english;
};

// Every code in tessdata-manifest.txt, in its order, so the two can be diffed.
//
// The endonyms are written the way the language writes them, including the
// script: that is the point of an endonym, and a transliterated one would be
// neither name.
const Entry kEntries[] = {
    { "afr",          "af", "Afrikaans",         "Afrikaans" },
    { "amh",          "am", "አማርኛ",              "Amharic" },
    { "ara",          "ar", "العربية",           "Arabic" },
    { "asm",          "as", "অসমীয়া",           "Assamese" },
    { "aze_cyrl",     "",   "Азәрбајҹан",        "Azerbaijani (Cyrillic)" },
    { "aze",          "az", "Azərbaycan",        "Azerbaijani" },
    { "bel",          "be", "Беларуская",        "Belarusian" },
    { "ben",          "bn", "বাংলা",             "Bengali" },
    { "bod",          "bo", "བོད་ཡིག",           "Tibetan" },
    { "bos",          "bs", "Bosanski",          "Bosnian" },
    { "bre",          "br", "Brezhoneg",         "Breton" },
    { "bul",          "bg", "Български",         "Bulgarian" },
    { "cat",          "ca", "Català",            "Catalan" },
    { "ceb",          "",   "Cebuano",           "Cebuano" },
    { "ces",          "cs", "Čeština",           "Czech" },
    { "chi_sim_vert", "",   "简体中文（竖排）",  "Chinese, Simplified, vertical" },
    { "chi_sim",      "zh", "简体中文",          "Chinese, Simplified" },
    { "chi_tra_vert", "",   "繁體中文（直排）",  "Chinese, Traditional, vertical" },
    { "chi_tra",      "",   "繁體中文",          "Chinese, Traditional" },
    { "chr",          "",   "ᏣᎳᎩ",               "Cherokee" },
    { "cos",          "co", "Corsu",             "Corsican" },
    { "cym",          "cy", "Cymraeg",           "Welsh" },
    { "dan",          "da", "Dansk",             "Danish" },
    { "deu_latf",     "",   "Deutsch (Fraktur)", "German, Fraktur" },
    { "deu",          "de", "Deutsch",           "German" },
    { "div",          "dv", "ދިވެހި",            "Dhivehi" },
    { "dzo",          "dz", "རྫོང་ཁ",            "Dzongkha" },
    { "ell",          "el", "Ελληνικά",          "Greek" },
    { "eng",          "en", "English",           "English" },
    { "enm",          "",   "Middle English",    "Middle English" },
    { "epo",          "eo", "Esperanto",         "Esperanto" },
    // Not a language: a model that finds mathematics on a page.
    { "equ",          "",   "Mathematics",       "Mathematics" },
    { "est",          "et", "Eesti",             "Estonian" },
    { "eus",          "eu", "Euskara",           "Basque" },
    { "fao",          "fo", "Føroyskt",          "Faroese" },
    { "fas",          "fa", "فارسی",             "Persian" },
    { "fil",          "",   "Filipino",          "Filipino" },
    { "fin",          "fi", "Suomi",             "Finnish" },
    { "fra",          "fr", "Français",          "French" },
    { "frk",          "",   "Fraktur",           "Frankish" },
    { "frm",          "",   "Moyen français",    "Middle French" },
    { "fry",          "fy", "Frysk",             "Western Frisian" },
    { "gla",          "gd", "Gàidhlig",          "Scottish Gaelic" },
    { "gle",          "ga", "Gaeilge",           "Irish" },
    { "glg",          "gl", "Galego",            "Galician" },
    { "grc",          "",   "Ἑλληνικά",          "Ancient Greek" },
    { "guj",          "gu", "ગુજરાતી",           "Gujarati" },
    { "hat",          "ht", "Kreyòl ayisyen",    "Haitian Creole" },
    { "heb",          "he", "עברית",             "Hebrew" },
    { "hin",          "hi", "हिन्दी",            "Hindi" },
    { "hrv",          "hr", "Hrvatski",          "Croatian" },
    { "hun",          "hu", "Magyar",            "Hungarian" },
    { "hye",          "hy", "Հայերեն",           "Armenian" },
    { "iku",          "iu", "ᐃᓄᒃᑎᑐᑦ",            "Inuktitut" },
    { "ind",          "id", "Bahasa Indonesia",  "Indonesian" },
    { "isl",          "is", "Íslenska",          "Icelandic" },
    { "ita_old",      "",   "Italiano antico",   "Old Italian" },
    { "ita",          "it", "Italiano",          "Italian" },
    { "jav",          "jv", "Basa Jawa",         "Javanese" },
    { "jpn_vert",     "",   "日本語（縦書き）",  "Japanese, vertical" },
    { "jpn",          "ja", "日本語",            "Japanese" },
    { "kan",          "kn", "ಕನ್ನಡ",             "Kannada" },
    { "kat_old",      "",   "ძველი ქართული",     "Old Georgian" },
    { "kat",          "ka", "ქართული",           "Georgian" },
    { "kaz",          "kk", "Қазақ",             "Kazakh" },
    { "khm",          "km", "ភាសាខ្មែរ",         "Khmer" },
    { "kir",          "ky", "Кыргызча",          "Kyrgyz" },
    { "kmr",          "ku", "Kurmancî",          "Kurdish, Kurmanji" },
    { "kor_vert",     "",   "한국어(세로쓰기)",  "Korean, vertical" },
    { "kor",          "ko", "한국어",            "Korean" },
    { "lao",          "lo", "ລາວ",               "Lao" },
    { "lat",          "la", "Latina",            "Latin" },
    { "lav",          "lv", "Latviešu",          "Latvian" },
    { "lit",          "lt", "Lietuvių",          "Lithuanian" },
    { "ltz",          "lb", "Lëtzebuergesch",    "Luxembourgish" },
    { "mal",          "ml", "മലയാളം",            "Malayalam" },
    { "mar",          "mr", "मराठी",             "Marathi" },
    { "mkd",          "mk", "Македонски",        "Macedonian" },
    { "mlt",          "mt", "Malti",             "Maltese" },
    { "mon",          "mn", "Монгол",            "Mongolian" },
    { "mri",          "mi", "Te reo Māori",      "Maori" },
    { "msa",          "ms", "Bahasa Melayu",     "Malay" },
    { "mya",          "my", "မြန်မာ",            "Burmese" },
    { "nep",          "ne", "नेपाली",            "Nepali" },
    { "nld",          "nl", "Nederlands",        "Dutch" },
    { "nor",          "no", "Norsk",             "Norwegian" },
    { "oci",          "oc", "Occitan",           "Occitan" },
    { "ori",          "or", "ଓଡ଼ିଆ",             "Odia" },
    // Not a language: orientation and script detection.
    { "osd",          "",   "Orientation",       "Orientation and script" },
    { "pan",          "pa", "ਪੰਜਾਬੀ",            "Punjabi" },
    { "pol",          "pl", "Polski",            "Polish" },
    { "por",          "pt", "Português",         "Portuguese" },
    { "pus",          "ps", "پښتو",              "Pashto" },
    { "que",          "qu", "Runa Simi",         "Quechua" },
    { "ron",          "ro", "Română",            "Romanian" },
    { "rus",          "ru", "Русский",           "Russian" },
    { "san",          "sa", "संस्कृतम्",         "Sanskrit" },
    { "sin",          "si", "සිංහල",             "Sinhala" },
    { "slk",          "sk", "Slovenčina",        "Slovak" },
    { "slv",          "sl", "Slovenščina",       "Slovenian" },
    { "snd",          "sd", "سنڌي",              "Sindhi" },
    { "spa_old",      "",   "Español antiguo",   "Old Spanish" },
    { "spa",          "es", "Español",           "Spanish" },
    { "sqi",          "sq", "Shqip",             "Albanian" },
    { "srp_latn",     "",   "Srpski",            "Serbian, Latin" },
    { "srp",          "sr", "Српски",            "Serbian" },
    { "sun",          "su", "Basa Sunda",        "Sundanese" },
    { "swa",          "sw", "Kiswahili",         "Swahili" },
    { "swe",          "sv", "Svenska",           "Swedish" },
    { "syr",          "",   "ܣܘܪܝܝܐ",            "Syriac" },
    { "tam",          "ta", "தமிழ்",             "Tamil" },
    { "tat",          "tt", "Татар",             "Tatar" },
    { "tel",          "te", "తెలుగు",            "Telugu" },
    { "tgk",          "tg", "Тоҷикӣ",            "Tajik" },
    { "tha",          "th", "ไทย",               "Thai" },
    { "tir",          "ti", "ትግርኛ",              "Tigrinya" },
    { "ton",          "to", "Lea faka-Tonga",    "Tongan" },
    { "tur",          "tr", "Türkçe",            "Turkish" },
    { "uig",          "ug", "ئۇيغۇرچە",          "Uyghur" },
    { "ukr",          "uk", "Українська",        "Ukrainian" },
    { "urd",          "ur", "اردو",              "Urdu" },
    { "uzb_cyrl",     "",   "Ўзбек",             "Uzbek (Cyrillic)" },
    { "uzb",          "uz", "Oʻzbek",            "Uzbek" },
    { "vie",          "vi", "Tiếng Việt",        "Vietnamese" },
    { "yid",          "yi", "ייִדיש",            "Yiddish" },
    { "yor",          "yo", "Yorùbá",            "Yoruba" },
};

// Built once. A linear scan of 126 entries per row of the picker is not slow, but
// the picker is rebuilt on every settings change and the map costs one page.
const QHash<QString, const Entry *> &index()
{
    static QHash<QString, const Entry *> map;
    if (map.isEmpty()) {
        for (const Entry &entry : kEntries) {
            map.insert(QString::fromLatin1(entry.code), &entry);
        }
    }
    return map;
}

const Entry *lookUp(const QString &code)
{
    return index().value(code.trimmed().toLower(), nullptr);
}

} // namespace

QString endonym(const QString &code)
{
    const Entry *entry = lookUp(code);
    return entry ? QString::fromUtf8(entry->endonym) : QString();
}

QString englishName(const QString &code)
{
    const Entry *entry = lookUp(code);
    return entry ? QString::fromUtf8(entry->english) : QString();
}

QString displayName(const QString &code)
{
    const Entry *entry = lookUp(code);
    if (!entry) {
        return code;
    }

    const QString own = QString::fromUtf8(entry->endonym);
    const QString english = QString::fromUtf8(entry->english);
    if (own == english) {
        return own;
    }
    return own + QStringLiteral(" (") + english + QLatin1Char(')');
}

QString codeForLocale(const QLocale &locale, const QStringList &available)
{
    // QLocale::name() is "fr_FR", "pt_BR", "zh_CN": the language is the part
    // before the underscore.
    QString wanted = locale.name().section(QLatin1Char('_'), 0, 0).toLower();
    if (wanted.isEmpty()) {
        return QString();
    }

    // Bokmal and Nynorsk both read with Tesseract's one Norwegian model, and a
    // Norwegian phone reports one of the two rather than plain "no".
    if (wanted == QLatin1String("nb") || wanted == QLatin1String("nn")) {
        wanted = QStringLiteral("no");
    }

    for (const Entry &entry : kEntries) {
        if (wanted != QLatin1String(entry.iso1)) {
            continue;
        }
        const QString code = QString::fromLatin1(entry.code);
        if (available.contains(code)) {
            return code;
        }
    }
    return QString();
}

bool isReadable(const QString &code)
{
    const QString normalised = code.trimmed().toLower();
    return normalised != QLatin1String("osd") && normalised != QLatin1String("equ");
}

} // namespace LanguageNames
