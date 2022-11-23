NOTES:
=======

The NSIS installer license display works best for other languages in RTF
file format (not TXT), and with free-form text (without forced line breaks).
Most TXT files will not display all characters correctly, so small file size
RTF files are preferred. The body text font size should be 10pt.



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

Spanish GPLv3 license unofficial translation was from: https://github.com/LSLSpanish/translation_GPLv3_to_spanish/tree/master/Versi%C3%B3n%20definitiva
Spanish [es]: https://github.com/LSLSpanish/translation_GPLv3_to_spanish/tree/master/Versi%C3%B3n%20definitiva/GPLv3-spanish.html