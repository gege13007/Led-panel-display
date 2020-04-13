PANNEAU A LED RGB PROGRAMMABLE sur Raspberry Pi
===============================================
Après avoir construit mon panneau lumineux (2 * 5 matrices 32*32 rgb) en utilisant le projet de Henner Zeller (hzeller) que je remercie encore pour son travail, je me suis aperçu que je ne pouvais pas facilement afficher de grosses lettres avec ses seuls programmes d'exemple (fontes bdf pas très commodes...). D'où l'idée de redéfinir des fontes sous formes d'images .gif (ou autres), et de créer une sorte de langage de commande comme du html, pour permettre toute sorte d'affichage.

Le petit utilitaire en VBasic 'FontCreator' permet de générer des fichiers Gif pour chaque lettre, en choisissant une police et en balayant d'un code Ascii à un autre. Il n'y a plus qu'à vérifier ou ajuster les gifs avec n'importe quel soft de retouche.

Le programme 'panelviewer.cc' qui permet d'afficher un texte enrichi directement (stocké dans message.txt) un peu à la manière d'un fichier html. Un système de tags permet de faire des effets ou de modifier les paramètres d'affichage.
Comme en xml, chaque commande est sous la forme :
   <FONCTION : paramètre>
   
(Si le ':' ou le '>' sont omis, la commande n'est pas valide et les caractères affichés).

Voici ce que cela peut donner dans 'message.txt' :

msg = "<CLEAR:0><SOLID:0><INK:c0c0c0><CADRE:1><FONT:comic48><CRENAGE:-5>Pizza<INK:22ff22>5€<PAUSE:1><PIXELISE:10><IMGFITW:pizza2.jpg><PAUSE:1>
<EXPLODE:5>"

Et voici la liste des fonctions déjà réalisées :

<BLUR:s> fait un effet de flou progressif vers le noir sur l'écran (x=vitesse)

<BACKCOLOR:rrggbb> définit la couleur de fond (rrggbb en hexa)

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

<FIXE:n> Permet de passer en mode fixe (=1 : pas de défilement de l'écran), ou défilant (=0 par défaut vers la gauche).

<FONT:txt> Nom du répertoire de la fonte choisie

<IMG:file> Affiche le fichier image au coin haut/gauche (pas de déformation, peut dépasser de l'écran). Supporte les Gif animés. Le curseur ne bouge pas. L'affichage des images tient compte de la transparence. Si SOLID=0 une couleur d'image égale à BACKCOLOR ne sera pas dessinée.

<IMGW:file> Affiche l'image file en ajustant la largeur à l'écran (ratio H/W conservé). Gifs animés compris. Le curseur ne bouge pas.

<IMGH:file> Affiche l'image file en ajustant la hauteur à l'écran (ratio H/W conservé). Gifs animés compris. Le curseur ne bouge pas.

<IMG+><IMGW+><IMGH+> idem que ci-dessus avec déplacement du curseur.

<IMGSWP:file> Affiche une image (pas de gif animé) sans déformation. Si elle est trop grande, fait un balayage (tournant) de la taille de l'écran.

<INK:rrggbb> Définit la couleur de l'encre du prochain car.

<INVERT:n> Fait un effet de négatif sur l'écran (n fois).

<LOOP:n> Si n>1 revient juste après le précédent <DO:>, et reboucle n fois au total. Utile pour répéter un gif animé...

<PAUSE:n> Pause du défilement... n secondes. 

<PIXELISE:s> Fait un effet de pixelisation jusqu'à vider l'écran (s=vitesse)

<RAINBOWBACK:n> Fait un effet d'arc en ciel sur le fond du texte (s=vitesse)

<RAINBOWINK:n> Fait un effet d'arc en ciel sur le texte (s=vitesse)

<RESET:x> Revient au début du message (permet de masquer la fin ?)

<ROTD:ang> Effet de rotation de l'écran de ang degrés sur la droite

<ROTG:ang> Effet de rotation de l'écran de ang degrés sur la gauche

<SETX:n> (et <SETY:n>) Positionne le pointeur d'écriture en X=n ou Y=n.

<SHAKE:n> Effet de tremblements (n fois)

<SLOW:n> Fait un ralenti progressif du défilement si n=1 (jusqu'à une PAUSE par exemple)

<SOLID:n> Dessine (solid=1) la couleur de fond des lettres ou pas (solid=0 - transparent).

<SPEED:n> Fixe la vitesse de défilement (et des effets). De 0 (+ rapide à 250 trop long).

<SPLASH1:file> Remplit tout l'écran avec un motif (file) au hasard.

<SPLASH2:file> Remplit tout l'écran avec un motif (file) en balayage horizontal.

<UP:n> Shifte l'écran n pixels vers le haut.

AMELIORATIONS PREVUES
=====================
- le support des vidéos.
- ecriture du txt sur plusieurs lignes
- l'écriture à 90° ?

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

Remerciements
=============
Encore une fois merci à H.Zeller pour son travail. Vous trouverez tout sur la construction et l'interfaçage des matrices de leds, ainsi que la librairie RGBmatrix sur https://github.com/hzeller.
Le prog utilise les librairies graphiques Magick++ de www.imagemagick.org/

