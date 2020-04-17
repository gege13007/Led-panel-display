BUILD A VERSATILE PROGRAMMABLE RGB DISPLAY PANEL
======================================================
After my first tests of led panel display building with 32x32 rgb leds matrix (with hzeller library - thanks to him) - I quickly realized that I couldn't draw so big characters to fit my panel display (the demo prog uses old bdf fonts).
My idea was also to implement a simple but diversified markup language allowing to cover a maximum of situations.
I began to build my own new fonts using small Gif images. It's now possible to write enriched texts, including some special commands to change the font, size, position, colors, and some special effects...
It's now possible to mix all kind of pictures, with text, scrolling or not...

I also made 'FontWriter' in VB6 to create my gifs font files for each characters. You just have to choose a font anf size. Enter the first char ascii code (like A) and the last ascii code (like Z). Then 'Ecrire' will create gif char from A to Z. You'll may be need to retouch them with your best drawing software. For now, the panel uses only the 8 bits direct ascii codes of the c langage compiler (from 32 to 128). The 128 code is re-affected to '€' char.

So 'panelviewer' (written in C) allows you to display enriched text (stored in file message.txt) in the same manner than html file. Some special tags allow to specify the ink and back colors, the position and size, and also to do some special transitions and effects.
Like with xml tags, each command must be written like this : <FONCTION : parameter>
(If ':' or '>' are omitted, the command is ignored and displayed like simple text).
The panel may use 2 differents kinds of color masks, one buffer for the colored border line, another buffer for a backgroud picture.

Here's a file example 'message.txt' :

<CLEAR:0><SOLID:0><INK:c0c0c0><CADRE:1><FONT:comic48><CRENAGE:-5>Pizza<INK:22ff22>5€<PAUSE:1><PIXELISE:10><IMGFITW:pizza2.jpg><PAUSE:1>
<EXPLODE:5><RESET:0>

Here is the syntax of all available commands :

<BLUR:s> display a blurring effect on all screen to the black color (s=speed)

<BACK:rrggbb> set backcolor for the next chars to display (rrggbb in hexa). The last specified backcolor is used in some special trabsparency effects.

<BACKGRD:file> set a fixed background picture. Could be cancelled with <BACKGRD:>

<BLINK:n> do n times blinking the text.

<CADRE:n> draw a small rectangle on the borders of the screen (thickness = n pixel). The rectangle is fix and always on the foreground, until you do <CADRE:0>. The border color is the last value of INKCOLOR.

<CLEAR:n> Clear the screen (set to BackColor). If n=0 the cursor position is reset to the left.

<CRENAGE:n> modify the character spacing +/- in pixels (-2 => draw 2 pixels on the left). It could be better to do SOLID=0. Transparency will avoid than the background color erase the previous character.

<DIVIDE:s> Effect of spliting the screen in two scrolling parts. Fill the center with Backcolor (s=speed)

<DO:> do a simple do...loop with some commands into. Could be useful to repeat an animated Gif...

<DOWN:n> scroll down entire screen n pixels (n=speed).

<EXPLODE:n> randomly exploding pixels effect (n = times)

<FADE:s> fade out the screen to a black background (s=speed)

<FIXE:n> Set the fixed mode when =1. So there is no more left scrolling. It's up to you to move the chars. The chars written out of the screen (right) could be lost or invisible. Default is 0 (auto-scroll to the left).

<FONDU:file> Draw a picture with a fade-out transition effect (don't resize).

<FONT:txt> Set the name of the font directory.

<FONTSIZE:n> Set the new font height to use for text. This cause resizing operation for all characters to display.

<IMG:file> Draw the picture 'file' to the current position (don't resize). Display animated Gifs. When SOLID=0, the backcolor could be transparent. Move the cursor position.

<IMGW:file> Draw the picture 'file' to the current position. Resize to fit screen width. Display animated Gifs. When SOLID=0, the backcolor could be transparent. Move the cursor position. When the height is bigger than screen, the picture is moved to the center.

<IMGH:file>  Draw the picture 'file' to the current position. Resize to fit screen Height. Display animated Gifs. When SOLID=0, the backcolor could be transparent. Move the cursor position.

<IMGSWP:file> Useful to draw a Big picture 'file' (no gifanim) without resizing operation. Do a visual scan effect right/down/left/up to show all parts of the picture on a small screen.

<INK:rrggbb> Set the Inkcolor (rrggbb hexadecimal values). Some effects use the last used Inkcolor.

<INVERT:n> Negative image effect on the screen (n times).

<LEFT:n> Scroll gradually the screen on the Left (n pixels).

<LOOP:n> If n>1 set back the text pointer to the previous <DO:>, and loops n times (if n=0 cause a never end loop). Useful to repeat a Gifanim or a portion of text.

<PAUSE:n> Pause n seconds.

<PIXELISE:s> Pixelisation effect of the screen (s=speed).

<RAINBOWBACK:n> Colored rainbow effect on the background (on the last Backcolor). s=speed.

<RAINBOWINK:n> Colored rainbow effect on the text (on the last Inkcolor). s=speed.

<RESET:x> Go back to the beginning of the text. Otherwise the program stops when reach the end of text.

<RIGHT:n> Scroll gradually the screen on the Right (n pixels).

<ROTD:ang> Visual rotation effect of ang degrees (clockwise).

<ROTG:ang> Visual rotation effect of de ang degrees (counter-clockwise).

<ROTSPEED:ang> Accelerating rotation effect of ang degrees.

<SETX:n> (et <SETY:n>) Set the writing pointer position in pixels on the screen (X=n or Y=n). The initial top Y position on the buffer is not 0 but screenH.

<SHAKE:n> Shaking effect (n times)

<SLOW:n> Slow down progressively the scrolling speed (if n=1). To do before PAUSE for example.

<SOLID:n> Dessine (solid=1) la couleur de fond des lettres ou pas (solid=0 - transparent).

<SPEED:n> Fixe la vitesse de défilement (et des effets). De 0 (+ rapide à 250 trop long).

<SPLASH1:file> Randomly fill all the screen with a picture 'file' (better with SOLID=0).

<SPLASH2:file> Fill all the screen with a picture 'file', doing an horizontal sweeping (better with SOLID=0).

<STEP:n> It's possible to modify the step (default 1) of pixel cols for each left scroll.

<TIME:x> display the hour hh:mm (may be later more options)

<TWIRL:ang> Twirl rotation effect (ang degrees).

<UP:n> Scroll up gradually the screen of n pixels.

SOME DEMOS VIDEOS
=================
The Pharmacy https://youtu.be/9tcXBY5XUHY

Images & effects https://youtu.be/1gwI9AUk74s 


WHAT TO DO ?
============
- support of video files (in progress).
- writing at 90° ?
- accept complex ascii codes ?

HOW TO USE
==========
Create your message to display in 'message.txt'.
Compile panelviewer.cc on you Raspberry PI with hzeller and Magicks++ Graphics dependencies and run.

make
sudo ./panelviewer 

As described by hzeller, use the option --led-no-hardware-pulse if your PI sound is active (or set 'dtparam-audio=off' in the file /boot/config.txt).

The file 'panel-config.txt' allows you to set some default display parameters. Some of them overload the initial inline options commands of  hzeller : 
* fonte = font name to use (must match to the font directory)
* led-chain = number of matrix for one chain GPIO (défaut 1)
* led-parallel = number of parallel chains in GPIO (défaut 1)
* led-rows = number of lines in one led matrix (default 32)
* led-cols = number of columns in one led matrix (default 32)

LIMITATIONS
===========
The prototype 160*64 pixels screen you can see on my videos run on a simple PI 3B+ in Raspbian windows mode. This is a quite speed cpu wich runs well with various effects, two parallel chains of 5 matrix. But it seems raisonable to set the maximum buffer size to an array of 512*512 in the code. A squarre is better for rotations effets...
But the maximal screen dimensions must be less, because the code needs to have a stock of available pixels to the right, up & down.
I consider to take for maximal Width = screenW + screenH, and for maximal Height = 3 * screenH.
The initial cursor position is (x=0, y=screenH). At the left and verticaly centered on the screen buffer.

Thanks
======
One more time thank to H.Zeller for its work. All the details and schematics to assemble your RGB Leds matrix are here https://github.com/hzeller.
The code uses also the Magick++ library www.imagemagick.org.

-------------------------------------------------------------------------------------------------------------------------------------

PANNEAU A LED RGB PROGRAMMABLE sur Raspberry Pi
===============================================
Après avoir construit mon panneau lumineux (2 * 5 matrices 32*32 rgb) en utilisant le projet de Henner Zeller (hzeller) que je remercie encore pour son travail, je me suis aperçu qu'il étais difficile d'afficher de grosses lettres avec le seul programme d'exemple disponible (fontes bdf pas très commodes...). D'où l'idée de construire un système plus complet permettant de redéfinir des fontes sous formes d'images .gif (ou autres), et de créer une sorte de langage de commande hypertexte comme du html, pour permettre toute sorte d'affichage, de choix des couleurs, d'effets spéciaux, etc...

Le programme en VBasic 'FontCreator' permet de vite générer des fichiers Gif pour chaque lettre, en choisissant une police et en balayant d'un code Ascii à un autre. Il n'y a plus qu'à vérifier ou ajuster les gifs avec n'importe quel soft de retouche.

Le programme 'panelviewer.cc' permet d'afficher un texte enrichi directement (stocké dans message.txt) un peu à la manière d'un fichier html. Un système de tags permet de faire des effets ou de modifier les paramètres d'affichage, les couleurs ou les images, et les effets.
Comme en xml, chaque commande est sous la forme : <FONCTION : paramètre>
   
(Si le ':' ou le '>' sont omis, la commande n'est pas valide et les caractères affichés).

Voici ce que cela peut donner dans 'message.txt' :

<CLEAR:0><SOLID:0><INK:c0c0c0><CADRE:1><FONT:comic48><CRENAGE:-5>Pizza<INK:22ff22>5€<PAUSE:1><PIXELISE:10><IMGFITW:pizza2.jpg><PAUSE:1>
<EXPLODE:5><RESET:0>

Et voici la liste des fonctions déjà réalisées :

<BLUR:s> fait un effet de flou progressif vers le noir sur l'écran (x=vitesse)

<BACK:rrggbb> définit la couleur de fond (rrggbb en hexa).C'est la dernière couleur de fond définie qui est utilisée avec les effets.

<BACKGRD:file> pour définir une image de fond, fixe et permanente. On peut arreter en faisant <BACKGRD:>

<BLINK:n> fait clignoter l'écran (n fois)

<CADRE:n> dessine un liseré (cadre) d'épaisseur n pixel. Un cadre est fixe, permanent et affiché en premier plan, jusqu'à ce qu'on fasse <CADRE:0>. La couleur du cadre est la valeur INKCOLOR au moment de la commande.

<CLEAR:n> Effacement de l'écran (à BackColor). Si n=0 le pointeur d'écriture revient à gauche.

<CRENAGE:n> Pour définir un écart +/- pixels entre chaque lettres (-2 pixels plus à gauche). Faire SOLID=0 pour avoir le fond des lettres transparent, sinon une lettre peut effacer la précédente.

<DIVIDE:s> Effet de division en deux de l'écran par le milieu jusqu'à Backcolor (s=vitesse)

<DO:> Marque le début d'une boucle <LOOP:n>. Utile pour rpéter un bloc de commandes, un Gif animé...

<DOWN:n> Shifte l'écran n pixels vers le bas.

<EXPLODE:n> Fait un effet d'explosion/mélange des pixels (n = longueur)

<FADE:s> Assombrit progressivement l'écran jusqu'au fond noir (s=vitesse)

<FIXE:n> Permet de passer en mode fixe (=1 : pas de défilement de l'écran), ou défilant (=0 par défaut vers la gauche). Attention, les caractères trop à droite sont perdus.

<FONDU:file> Affiche une image avec une transition en fondu/enchainé (pas de déformation, peut dépasser de l'écran).

<FONT:txt> Nom du répertoire de la fonte choisie.

<FONTSIZE:n> Permet de resizer la fonte en cours. Donne la nouvelle hauteur en pixels. Attention : opération calcul de resize en plus à chaque affichage de caractère.

<IMG:file> Affiche le fichier image au coin haut/gauche (pas de déformation, peut dépasser de l'écran). Supporte les Gif animés. Le curseur ne bouge pas. L'affichage des images tient compte de la transparence. Si SOLID=0 une couleur d'image égale à BACKCOLOR ne sera pas dessinée. A chaque dessin d'une image le curseur en X est avancé.

<IMGW:file> Affiche l'image file en ajustant la largeur à l'écran (ratio H/W conservé). Gifs animés compris. Une image qui dépasse l'écran en hauteur est recentrée en Y.

<IMGH:file> Affiche l'image file en ajustant la hauteur à l'écran (ratio H/W conservé). Gifs animés compris.

<IMGSWP:file> Affiche une image (pas de gif animé) sans déformation. Si elle est trop grande, fait un balayage (tournant) de la taille de l'écran.

<INK:rrggbb> Définit la couleur de l'encre du prochain car. C'est la dernière couleur INK qui est utilisée dans certains effets.

<INVERT:n> Fait un effet de négatif sur l'écran (n fois).

<LEFT:n> Décale tout l'écran de n pixels à gauche.

<LOOP:n> Si n>1 revient juste après le précédent <DO:>, et reboucle n fois au total. Si n=0 boucle infini. Utile pour répéter une portion de texte ou un Gif animé.

<PAUSE:n> Pause de n secondes. 

<PIXELISE:s> Fait un effet de pixelisation jusqu'à vider l'écran (s=vitesse)

<RAINBOWBACK:n> Fait un effet d'arc en ciel sur le fond du texte (n=cycles). Utilise la couleur de fond en cours.

<RAINBOWINK:n> Fait un effet d'arc en ciel sur le texte (n=cycles). Utilise la couleur INK en cours.

<RESET:x> Revient au début du message, sinon le programme s'arrête à la fin du message.

<RIGHT:n> Décale tout l'écran de n pixels vers la droite.

<ROTD:ang> Effet de rotation de l'écran de ang degrés sur la droite

<ROTG:ang> Effet de rotation de l'écran de ang degrés sur la gauche

<ROTSPEED:ang> Effet de rotation en Accélèration de ang degrés.

<SETX:n> (et <SETY:n>) Positionne le pointeur d'écriture en X=n ou Y=n. En Y, le buffer d'écran garde une marge de une hauteur d'écran en haut et en bas. Le coin haut/gauche est donc à 0/screenH.

<SHAKE:n> Effet de tremblements (n fois)

<SLOW:n> Fait un ralenti progressif du défilement si n=1 (avant un arrêt PAUSE).

<SOLID:n> Dessine (solid=1) la couleur de fond des lettres ou pas (solid=0 - transparent).

<SPEED:n> Fixe la vitesse de défilement (et des effets). De 0 (+ rapide à 250 trop long).

<SPLASH1:file> Remplit tout l'écran avec un motif (file) au hasard.

<SPLASH2:file> Remplit tout l'écran avec un motif (file) en balayage horizontal.

<TIME:x> Affiche l'heure 'hh:mm' courant de votre PI.

<TWIRL:ang> Effet de rotation en Tourbillon déformant sur l'écran de ang degrés.

<UP:n> Shifte l'écran n pixels vers le haut.

EXEMPLES VIDEOS DEMOS
=====================
La pharmacie https://youtu.be/9tcXBY5XUHY

Images et effets https://youtu.be/1gwI9AUk74s 


AMELIORATIONS PREVUES
=====================
- le support des vidéos.
- l'écriture à 90° ?
- codes ascii plus complexes

UTILISATION
===========
Faire sudo ./panelviewer 
( ajouter --led-no-hardware-pulse si la sortie son de votre PI est active, ou alors 'dtparam-audio=off' dans /boot/config.txt')

Le fichier 'panel-config.txt' contient certains paramètres par défaut chargés au démarrage, et qui peuvent remplacer les paramètres en-ligne initiaux du système créé par HZeller : 
* fonte = nom de la fonte au démarrage (correspond au nom de dossier)
* led-chain = nb de matrices en série par ligne de commande GPIO paramètre (défaut 1)
* led-parallel = nb de lignes GPIO en parallèle (défaut 1)
* led-rows = nb de lignes de leds par carreau (défaut 32)
* led-cols = nb de colonnes de leds par carreau (défaut 32)

LIMITATIONS
===========
Le proto toune sur une carte PI 3b+ (donc assez rapide) avec un écran de 160*64 pixels (5*32 / 2*32), piloté sur deux lignes parallèles. La vitesse est suffisante dans ce cas, veci dit il m'a paru raisonnable de limiter le buffer à une taille max de 512*512 en dur dans le programme. Une taille carrée est utile pour la rotation d'image...
Pour avoir de la marge, on essaie d'avoir un buffer d'au moins 3 * hauteurs d'écran (3*screenH); et large comme screenW+screenH. L'écriture se fait par défaut à la position x=0, y=screenH (à une hauteur d'écran) : au milieu à gauche dans le buffer.

Remerciements
=============
Encore une fois merci à H.Zeller pour son travail. Vous trouverez tout sur la construction et l'interfaçage des matrices de leds, ainsi que la librairie RGBmatrix sur https://github.com/hzeller.
Le prog utilise les librairies graphiques Magick++ de www.imagemagick.org/
