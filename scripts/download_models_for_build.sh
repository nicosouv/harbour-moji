#!/bin/bash
# Fetches the Tesseract language data that gets bundled into the RPM.
#
# Run before packaging. CI runs it on every build; the files are never committed,
# so the repo carries no binaries, but the installed app carries every byte it
# needs and never asks the network for anything.
#
# Pinned to a tag of tessdata_fast rather than to master, and every file verified
# against a SHA256 recorded here. "fast" is the integer-quantised set: a few
# megabytes per language instead of the ~15MB of "best", and on a phone CPU it is
# also several times quicker for a difference in accuracy you have to look for.
#
# To add a language: add it to LANGUAGES, run this, and paste the SHA256 the
# script prints for it. Do not guess a checksum - an unverified download is worse
# than no check at all, because it looks like one.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESSDATA_DIR="$SCRIPT_DIR/../tessdata"

TESSDATA_TAG="4.1.0"
BASE_URL="https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/${TESSDATA_TAG}"

# What the base package can read. Thirty languages, 80MB.
#
# Chosen to cover the writing a European phone actually meets - all of Western,
# Central and Eastern Europe, the Nordics - plus the world's most photographed
# non-Latin scripts: Arabic, Hebrew, both Chinese, Japanese, Korean, Hindi, Thai,
# Vietnamese.
#
# Per-language rather than per-script, deliberately. Tesseract also ships
# script-level models, and script/Latin.traineddata covers every Latin-script
# language in one file - but it is 85MB on its own, more than all thirty of these
# together. Script models only start paying for themselves past roughly 25
# languages of a single script.
#
# osd is not here, though it is tempting: it does orientation and script
# detection, and at 10.1MB it would be the largest single file in the package - an
# eighth of the total, for something that is not a language. A photo's rotation
# can be had more cheaply: OpenCV finds the dominant text angle, and where that is
# ambiguous, recognising at two rotations and keeping the higher mean confidence
# settles it. osd ships as a pack for anyone who wants the real thing.
LANGUAGES="eng fra deu spa ita por nld pol rus ukr tur ell ces swe dan nor fin hun ron cat ara heb jpn chi_sim chi_tra kor hin tha vie ind"

# The other ~96 languages, and osd, ship as separate harbour-moji-lang-* RPMs.
# All 126 at once is 339MB, which is not an app.
#
# Note this puts no network request inside Moji. The app never connects; the
# package manager fetches a pack, which is how every distribution has shipped
# dictionaries and locales for thirty years.

# sha256, recorded from tessdata_fast 4.1.0.
sha_for() {
    case "$1" in
        eng)      echo "7d4322bd2a7749724879683fc3912cb542f19906c83bcc1a52132556427170b2" ;;
        fra)      echo "ced037562e8c80c13122dece28dd477d399af80911a28791a66a63ac1e3445ca" ;;
        deu)      echo "19d219bbb6672c869d20a9636c6816a81eb9a71796cb93ebe0cb1530e2cdb22d" ;;
        spa)      echo "6f2e04d02774a18f01bed44b1111f2cd7f3ba7ac9dc4373cd3f898a40ea6b464" ;;
        ita)      echo "b8f89e1e785118dac4d51ae042c029a64edb5c3ee42ef73027a6d412748d8827" ;;
        por)      echo "c4932b937207a9514b7514d518b931a99938c02a28a5a5a553f8599ed58b7deb" ;;
        nld)      echo "ced0e5e046a84c908a6aa7accbef9a232c4a5d9a8276691b81c6ee64d02963f6" ;;
        pol)      echo "c4476cdbc0e33d898d32345122b7be1cbf85ace15f920f06c7714756e1ef79b2" ;;
        rus)      echo "e16e5e036cce1d9ec2b00063cf8b54472625b9e14d893a169e2b0dedeb4df225" ;;
        ukr)      echo "d59e53e2bded32f4445f124b4b00240fcac7e8044c003ab822ccb94f0b3db59b" ;;
        tur)      echo "7393381111e1152420fc4092cb44eef4237580d21b92bf30d7d221aad192c6b7" ;;
        ell)      echo "4fba8a0b461038d51f1c20d043d4f2ac38c4e778f1b90830847f7bd8fa3ba726" ;;
        ces)      echo "934bcaf97ef3348413263331131c9fa7f55f30db333c711929c124fb635f7e1b" ;;
        swe)      echo "f7304988d41f833efebcc2d529df54b1903ecebbc3da1faabd19a0fddd4fe586" ;;
        dan)      echo "acb1fd074487a31d1294fcdfd7d7c673467ffd8aeacb2ccd61ebcbf04eb4e2fa" ;;
        nor)      echo "0451eb4f8049ae78196806bf878a389a2f40f1386fe038568cf4441226ba6ef2" ;;
        fin)      echo "61a04cd62b507c3d9ae0e1cda399e6715ebf49dea9df47897c8acdcd3bd3e13c" ;;
        hun)      echo "35067e7cfe102dcdc953f9a758fdfaa6296b17a1ee6d874ee780fa306430b9fb" ;;
        ron)      echo "9adfde6b51ba4b97efd10ea37c3070fd3fc2bad7815e81f5c3c198cd96216cc9" ;;
        cat)      echo "250db73cd5b380d2798581295dc12f20d0828cdb335a65d833d12dfdbf57117d" ;;
        ara)      echo "e3206d3dc87fd50c24a0fb9f01838615911d25168f4e64415244b67d2bb3e729" ;;
        heb)      echo "11f9e43ab227f786352a50f75c94c2e9906f1baba86d93276da19da7ce0904db" ;;
        jpn)      echo "1f5de9236d2e85f5fdf4b3c500f2d4926f8d9449f28f5394472d9e8d83b91b4d" ;;
        chi_sim)  echo "a5fcb6f0db1e1d6d8522f39db4e848f05984669172e584e8d76b6b3141e1f730" ;;
        chi_tra)  echo "529c5b5797d64b126065cd55f2bb4c7fd7b15790798091b1ff259941a829330b" ;;
        kor)      echo "6b85e11d9bbf07863b97b3523b1b112844c43e713df8b66418a081fd1060b3b2" ;;
        hin)      echo "4c73ffc59d497c186b19d1e90f5d721d678ea6b2e277b719bee4e2af12271825" ;;
        tha)      echo "294227cc2d1292b0acb28d61d4115c88252b96d466ca90b417cf4cf0c67bf07c" ;;
        vie)      echo "79df64caf7bcfb2a27df5042ecb6121e196eada34da774956995747636d5bfa1" ;;
        ind)      echo "69786901da87ab8766c1ea7fbb10b28f2110c14da3f6c8f2735df131fba95d88" ;;
        osd)      echo "9cf5d576fcc47564f11265841e5ca839001e7e6f38ff7f7aacf46d15a96b00ff" ;;
        *)        echo "" ;;
    esac
}

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    else
        shasum -a 256 "$1" | cut -d' ' -f1
    fi
}

mkdir -p "$TESSDATA_DIR"

total_bytes=0
for lang in $LANGUAGES; do
    file="$TESSDATA_DIR/${lang}.traineddata"
    expected="$(sha_for "$lang")"

    if [ -z "$expected" ]; then
        echo "No checksum recorded for '$lang'." >&2
        echo "Download it, then add its SHA256 to sha_for():" >&2
        echo "  curl -fL $BASE_URL/${lang}.traineddata | sha256sum" >&2
        exit 1
    fi

    if [ -f "$file" ] && [ "$(sha256_of "$file")" = "$expected" ]; then
        echo "ok       ${lang}.traineddata (cached)"
    else
        echo -n "fetching ${lang}.traineddata ... "
        curl -sfL "$BASE_URL/${lang}.traineddata" -o "$file"

        actual="$(sha256_of "$file")"
        if [ "$actual" != "$expected" ]; then
            echo "CHECKSUM MISMATCH"
            echo "  expected: $expected" >&2
            echo "  actual:   $actual" >&2
            # Removed, so a retry cannot silently build against a bad file.
            rm -f "$file"
            exit 1
        fi
        echo "verified"
    fi

    bytes=$(wc -c < "$file")
    total_bytes=$((total_bytes + bytes))
done

echo
echo "$(echo "$LANGUAGES" | wc -w | tr -d ' ') files, $((total_bytes / 1024 / 1024))MB of language data ready in tessdata/"
echo "(base package only - other languages ship as harbour-moji-lang-* RPMs)"
