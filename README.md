# Companion IO
**Afficheur déporté pour MSunPV**

![Screenshot](img/IMG_6869.jpg) ![Screenshot](img/IMG_6871.jpg)

**Companion IO** est un écran déporté permetant de suivre la production de panneaux photovoltaiques, la consommation de du domicile, et la recharge solaire du ballon d'eau chaude par el routeur solaire **M'SunPV** de Ard-Tek, ainsi que divers informations complementaires tres utiles à l'optimisation
de l'autoconsomation.

**Companion IO** poursuit la voie initiée par **JJHontebeyrie** et **djeje12** avec leurs versions de Companion pour MSunPV et MaxPV.

Cette version est une réécriture presque complete, et un portage vers l'environnement de développement VisualC/PlatformIO. 

Elle permet :
 * l'affichage des mesures du routeur MSunPV
   * production des panneaux solaires
   * consommation ou injection sur le réseaux électrique
 * le taux de routage vers le ballon d'eau chaude
 * l'affichage du code couleur Tempo J et J+1
 * la temperature ballon ESC (si sonde presente)
 * la méteo et température via OpenWeatherMap
 * la prevision de production solaire à J et J+1 via 
 * la mise en veille et contrôle de luminosité de l'écran
 * la puissance du réseau Wifi
 * la consultation possible d'une page web locale affichant les mesures du routeur 
 * l'accès au serveur web local via l'url http://companion.local dans un navigateur (mDNS)
 * l'enregistrement possible des mesures par InfluxDB
 * l'intégration possible avec Home Assistant par MQTT et auto configuration (discovery)
 * la configuration en ligne du Wifi, et de tous les paramètres (AsyncWifiManager)

Cet afficheur est conçu pour être utilisé avec le **LILYGO T-Display S3**, et l'environnement de developpement **Platform.IO** sur **Visual Studio Code**.


Lien pour le Lilygo T-Display-S3 : https://lilygo.cc/products/t-display-s3. Achat: https://s.click.aliexpress.com/e/_DBC5gbz

L'affichage de la météo nécéssite une clef API OpenWeatherMap.org gratuite. Pour cela, créez un compte gratuit (sans CB) puis ensuite crér une clé d'API (https://home.openweathermap.org/api_keys). Puis, utilisez cette clé lors de la configuration du **Companion IO**.


Si **Companion IO** n'est pas configuré, ou après un double reset à moins de 15s d'intervale, l'afficheur active un point d'acces Wifi AP "Companion-IO", et passe en mode configuration. Un ecran sur fond bleu est affiché avec les information nécessaires à la connection. 
Vous pouvez alors vous connecter sans mot de passe à ce point d'accès temporaire, à l'aide d'un smartphone, une tablette, ou un PC. Connectez vous en suite avec le navigateur web à http://192.168.4.1, comme indiquer sur l'écran de l'afficheur. Le menu **Configuration** permet la saisie des identifiants Wifi, et des autres parametres de l'afficheur **Companion IO**.


**Fonctions de l'afficheur**

Cette version permer l'affichage de :
* La production photovoltaiques instantanée
* L'énergie routée vers le cumulus
* La consommation electrique du domicile
* la quantité d'énergie exportée
* Les informations météo locales
* L'heure et la date locale (synchronisée sur Internet)
* L'heure du lever et du coucher de soleil


![Screenshot](img/affiche.jpeg) 

Le rafraichissement des données photovoltaiques se fait toutes les 15 secondes, celui de la météo toutes les 15 minutes.

Le bouton du T-Diplay situé en bas (si vous avez branché le module par la gauche), permet de sélectionner en séquence la l'écran affiché (affichage principal, Météo, Prévision de production, Tempo, Cumuls, Réglage luninosité).

Le bouton du T-Diplay situé en haut permet d'allumer ou éteindre l'écran. Celui-ci s'éteindra également au bout de 2mn si vous avez choisi le mode veille lors de la configuration. Sur l'écran de réglage de la luminosité de l'écran, un simple clic sur ce bouton augmente la luminosité, alors qu'un double clic la diminue.

En cas de problème, le bouton reset situé sur le haut du boitier permet de relancer le programme. Un double reset en moins de 15s permet d'activer le mode de configuration (Wifi AP).

**MSunPV**

Le *MSunPV* est un routeur solaire permettant d'utiliser l'éxèdent de production solaire des panneaux pour recharger ballon d'eau chaude au lieu de l'injecter sur le réseau.
Tous les détails sont sur le site d'Ard-Tek (https://ard-tek.com).

![Screenshot](img/SAM_0251_640.JPG)

**Configuration Wifi et parametrage**

Le mode de configuration est activé au démarage si le reseau Wifi configurer n'est pas joignable, ou si deux resets sont effectués à moins de 10s d'intervale.
L'écran de l'afficheur, indique alors le nom du Wifi (Companion-NG) et l'adresse (http://192.168.4.1) à laquelle vous devez vous connecter avec un smartphone, une tablette, ou un PC. Renseigner en suite le nom de votre Wifi et son mot de passe, ainsi que les autre parametres de configuration propre au Companion (adresse du routeur MSunPV, puissance des paneaux solaire et du cumulus, presence ou non d'une sonde de temperature du cumulus, longitude et latitude de votre maison pour les infos météo, et clé d'API OpenWeather.map). Apres validation des parametres, le Companion effectue un reset et tente de se connecter au Wifi configuré.
Si la connexion au Wifi est réussie, l'écran de demarage du Companion affiche l'adresse IP du Companion. Celui si peut alors être joint depuis votre réseau local à cette adresse, ou avec l'url : http://companion.local

**Utilisation avec Plateform.IO**

* Installer PlatformIO : 
  * Voir : https://platformio.org/platformio-ide
    * Download and install official Microsoft's Visual Studio Code, PlatformIO IDE is built on top of it
    * Open VSCode Extension Manager
    * Search for official PlatformIO IDE extension
    * Install PlatformIO IDE
* Cloner le dépots du projet, dans le répertoire de travail
* ou télécharger le zip des sources du projet, et le déziper dans le répertoire de travail 

**Crédits**

L'afficheur est basé sur les travaux de Companion MSunPV 2.50 de @jjhontebeyrie (https://github.com/JJHontebeyrie/Companion), mais egalement ceux du Companion pour MaxPV de @djeje12 (https://github.com/djeje12/Companion_for_MaxPV).

Le code a été très largement ré-écrit et adpaté.


**Intégration avec InfluDB**

TODO Description

**Intégration avec Home Assistant**

TODO Description
