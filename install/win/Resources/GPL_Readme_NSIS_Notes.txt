NOTES:
=======

The NSIS installer license display works best for other languages in RTF
file format (not TXT), and with free-form text (without forced line breaks).
Most TXT files will not display all characters correctly, so small file size
RTF files are preferred. The body text font size should be 10pt (or as low as 7pt).

Process Outline: Using MS Word with a blank document with 'Narrow' page margins works best.
Copy and paste the text into the document. Select the entire text and decrease the font to make the body text 10pt.
Save the file as RTF. Try opening a separate copy of the file with LibreOffice Writer, and re-save the RTF.
Use whichever RTF has the smaller file size.


==========================================================
NSIS compiler setup change to fix the path separators from
being displayed as Yen symbols for Japanese:

In file: "${NSISDIR}\Contrib\Language files\Japanese.nlf"

Make a backup copy, and update the original from:
=============
# Font and size - dash (-) means default
ＭＳ Ｐゴシック
9
=============

To:
=============
# Font and size - dash (-) means default
-
-
=============

After that change, the path looks normal
See: http://forums.winamp.com/showthread.php?t=278514
==========================================================



GPLv3 license unofficial translations are from the links at: https://www.gnu.org/licenses/translations.html
Czech [cs]: https://jxself.org/translations/gpl-3.cz.shtml
Danish [da]: https://raw.githubusercontent.com/scootergrisen/licenser/master/gpl-3.0.da.txt
German [de]: http://www.gnu.de/documents/gpl-3.0.de.html
Basque [eu]: https://www.euskadi.eus/contenidos/informacion/software_lizentziak_2018/eu_def/adjuntos/gnu-gpl-v3.eu.html
Japanese [ja]: http://gpl.mhatta.org/gpl.ja.html
Italian [it]: http://homeunix.katolaz.net/gplv3/gplv3-it-final.html
Dutch [nl]: http://bartbeuving.files.wordpress.com/2008/07/gpl-v3-nl-101.pdf
PortugueseBR [pt-br]: http://licencas.softwarelivre.org/gpl-3.0.pt-br.odt
Romanian [ro]: http://staff.cs.upt.ro/~gnu/Licenta_GPL-3-0_RO.odt
Russian [ru]: http://antirao.ru/gpltrans/gplru.pdf
Slovak [sk]: http://jxself.org/translations/gpl-3.sk.shtml
Serbian [sr]: http://alas.matf.bg.ac.rs/~mr99164/ojl-3.0.html
SimpChinese [zh-cn]: https://jxself.org/translations/gpl-3.zh.shtml
TradChinese [zh-tw]: http://www.chinasona.org/gnu/gnuv3-tc.html

Finnish GPLv3 license unofficial translation was from: http://gnu.ist.utl.pt/licenses/translations.html#translations
Finnish [fi]: http://www.turre.com/licenses/gpl_fi.php

Spanish GPLv3 license unofficial translation was from: https://lslspanish.github.io/translation_GPLv3_to_spanish/
Spanish [es]: https://github.com/LSLSpanish/translation_GPLv3_to_spanish/tree/master/Versi%C3%B3n%20definitiva/GPLv3-spanish.html

GPLv3 license unofficial translations are from the links at: https://libreplanet.org/wiki/Group:GPL_Translators
Lithuanian [lt]: https://libreplanet.org/wiki/Group:GPL_Translators/Lithuanian/GPLv3
Greek [el]: https://libreplanet.org/wiki/Group:GPL_Translators/Greek/GPLv3

Hungarian [hu]: https://gnu.hu/gplv3.html

==========================================================
NSIS custom installer page text was initially translated
mostly using Google Translate. These were the mappings used:

Google Translate  --  NSIS
---------------------------
Spanish  -->  Spanish & SpanishInternational
Norwegian --> Norwegian & NorwegianNynorsk
Portuguese  -->  Portuguese & PortugueseBR
Serbian  -->  Serbian
SerbianLatin  -->  Latin
Persian  -->  Farsi
[DNE], used https://www.stars21.com/translator/english/breton/  --> Breton
Kurdish (Kurmanji) --> Kurdish
[DNE], used https://www.stars21.com/translator/english/asturian/  --> Asturian



---------------------------------
Supported languages in NSIS (67):
---------------------------------
English
French
German
Spanish
SpanishInternational
SimpChinese
TradChinese
Japanese
Korean
Italian
Dutch
Danish
Swedish
Norwegian
NorwegianNynorsk
Finnish
Greek
Russian
Portuguese
PortugueseBR
Polish
Ukrainian
Czech
Slovak
Croatian
Bulgarian
Hungarian
Thai
Romanian
Latvian
Macedonian
Estonian
Turkish
Lithuanian
Slovenian
Serbian
SerbianLatin
Arabic
Farsi
Hebrew
Indonesian
Mongolian
Luxembourgish
Albanian
Breton
Belarusian
Icelandic
Malay
Bosnian
Kurdish
Irish
Uzbek
Galician
Afrikaans
Catalan
Esperanto
Asturian
Basque
Pashto
ScotsGaelic
Georgian
Vietnamese
Welsh
Armenian
Corsican
Tatar
Hindi
