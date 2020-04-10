PANNEAU A LED PROGRAMMABLE sur Raspberry Pi GPIO
================================================
Après avoir construit mon panneau lumineux (2 * 5 matrices 32*32 rgb) en utilisant le projet de Henner Zeller (hzeller) que je remercie encore, je me suis aperçu que je ne pouvais pas facilement afficher de grosses lettres ou faire un peu n'importe quoi avec les seuls programmes d'exemple.


<BLUR:s> fait un effet de flou progressif vers le noir sur l'écran (x=vitesse)
<BACKCOLOR:rrggbb> définit la couleur de fond (rrggbb en hexa)
<BACKGRD:file> pour définir une image de fond, fixe et permanente. On peut arreter en faisant <BACKGRD:>
<BLINK:n> fait clignoter l'écran (n fois)
<CADRE:n> dessine un liseré (cadre) d'épaisseur n pixel, et de couleur INKCOLOR. Il est fixe, permanent et affiché sur tout le reste, jusqu'à ce qu'on fasse <CADRE:0>.
<CLEAR:n> Effacement de l'écran (à BackColor). Si n=0 le pointeur d'écriture revient à gauche.
<CRENAGE:n> Pour définir un écart +/- pixels entre chaque lettres (-2 serre de 2 pixels plus à gauche). Faire SOLID=0 pour avoir le fond des lettres transparent, sinon une lettre peut effacer la précédente.
<DIVIDE:s> Effet de division en deux de l'écran par le milieu jusqu'à Backcolor (s=vitesse)
<DOWN:n> Shifte l'écran n pixels vers le bas.
<EXPLODE:n> Fait un effet d'explosion/mélange des pixels (n = longueur)
<FADE:s> Assombrit progressivement l'écran jusqu'au fond noir (s=vitesse)
<FIXE:n> Permet de passer en mode fixe (=1 : pas de défilement de l'écran), ou défilant (=0 par défaut vers la gauche).
<FONT:txt> Nom du répertoire de la fonte choisie
<IMG:file> Affiche le fichier image au coin haut/gauche (pas de déformation, peut dépasser de l'écran).
<IMGFITW:file> Affiche l'image file en ajustant la largeur à l'écran (ratio H/W conservé).
<IMGFITH:file> Affiche l'image file en ajustant la hauteur à l'écran (ratio H/W conservé).
<IMGSWP:file> Affiche une image sans déformation. Si elle est trop grande, fait un balayage (tournant) de la taille de l'écran.
<INK:rrggbb> Définit la couleur de l'encre du prochain car.
<INVERT:n> Fait un effet de négatif sur l'écran (n fois).
<PAUSE:n> Pause du défilement... n secondes.
<PIXELISE:s> Fait un effet de pixelisation jusqu'à vider l'écran (s=vitesse)
<RAINBOWBACK:n> Fait un effet d'arc en ciel sur le fond du texte (s=vitesse)
<RAINBOWINK:n> Fait un effet d'arc en ciel sur le texte (s=vitesse)
<RAZ:x> Revient au début du message (permet de masquer la fin ?)
<ROTD:ang> Effet de rotation de l'écran de ang degrés sur la droite
<ROTG:ang> Effet de rotation de l'écran de ang degrés sur la gauche
<SHAKE:n> Effet de tremblements (n fois)
<SLOW:n> Fait un ralenti progressif du défilement si n=1 (jusqu'à une PAUSE par exemple)
<SOLID:n> Dessine (solid=1) la couleur de fond des lettres ou pas (solid=0 - transparent).
<SPEED:n> Fixe la vitesse de défilement (et des effets). De 0 (+ rapide à 250 trop long).
<SPLASH1:file> Remplit tout l'écran avec un motif (file) au hasard.
<SPLASH2:file> Remplit tout l'écran avec un motif (file) en balayage horizontal.
<UP:n> Shifte l'écran n pixels vers le haut.
