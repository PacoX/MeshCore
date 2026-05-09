# MeshCore sur Seeed XIAO S3 avec WireGuard

Ce guide décrit la variante WireGuard pour le XIAO S3 du dépôt MeshCore et la manière de compiler puis flasher le firmware avec PlatformIO.

L’environnement à utiliser est Xiao_S3_WIO_companion_radio_wireguard, défini dans [variants/xiao_s3_wio/platformio.ini](../variants/xiao_s3_wio/platformio.ini#L110).

## 1. Ce qu’il faut modifier

La configuration WireGuard attend des valeurs réelles pour les paramètres WiFi et VPN. Les clés et l’adresse du serveur sont passées via les macros suivantes, visibles dans [variants/xiao_s3_wio/platformio.ini](../variants/xiao_s3_wio/platformio.ini#L117):

    WIFI_SSID
    WIFI_PWD
    WG_LOCAL_IP
    WG_PRIVATE_KEY
    WG_PEER_PUBLIC_KEY
    WG_ENDPOINT_ADDRESS
    WG_ENDPOINT_PORT
    WG_NTP_SERVER
    WG_NTP_TIMEOUT_MS

Les valeurs par défaut et les vérifications associées sont aussi décrites dans [variants/xiao_s3_wio/WireGuardSettings.h](../variants/xiao_s3_wio/WireGuardSettings.h).

### Générer les clés WireGuard

Sur une machine Linux avec les outils WireGuard installés :

    wg genkey | tee privatekey | wg pubkey > publickey

Ensuite, utilise le contenu de privatekey comme valeur de WG_PRIVATE_KEY et le contenu de publickey comme clé publique du pair côté serveur.

## 2. Exemple de configuration

Dans l’environnement Xiao_S3_WIO_companion_radio_wireguard, les valeurs par défaut ressemblent à ceci :

    WIFI_SSID = "myssid"
    WIFI_PWD = "password"
    WG_LOCAL_IP = "10.0.0.2"
    WG_ENDPOINT_ADDRESS = "vpn.example.com"
    WG_ENDPOINT_PORT = 51820
    WG_NTP_SERVER = "pool.ntp.org"
    WG_NTP_TIMEOUT_MS = 15000

À remplacer avant compilation :

    WG_PRIVATE_KEY
    WG_PEER_PUBLIC_KEY

Le firmware active WireGuard avec WIREGUARD_ENABLED=1 et ajoute la dépendance WireGuard-ESP32-Arduino, visible dans [variants/xiao_s3_wio/platformio.ini](../variants/xiao_s3_wio/platformio.ini#L117).

## 3. Compiler avec PlatformIO

Depuis la racine du dépôt, compile l’environnement WireGuard du XIAO S3 :

    pio run -e Xiao_S3_WIO_companion_radio_wireguard

Si tu préfères garder les secrets hors du fichier versionné, crée un platformio.local.ini à la racine et surcharge uniquement cet environnement avec tes vraies valeurs.

## 4. Flasher sur la carte

La version simple consiste à lancer la compilation puis l’upload en une seule commande :

    pio run -e Xiao_S3_WIO_companion_radio_wireguard -t upload

Si PlatformIO ne trouve pas le port automatiquement, précise le port série de la carte :

    pio run -e Xiao_S3_WIO_companion_radio_wireguard -t upload --upload-port /dev/ttyACM0

Sur XIAO S3, si l’upload échoue au moment du reset, maintiens le bouton BOOT pendant la connexion USB ou pendant le début du flash, puis relance l’upload.

## 5. Vérifier que tout démarre

Ouvre le moniteur série à 115200 bauds :

    pio device monitor -b 115200

Le firmware doit ensuite :

1. se connecter au WiFi,
2. synchroniser l’heure NTP,
3. établir le tunnel WireGuard,
4. lancer le service MeshCore au-dessus du tunnel.

## 6. Côté serveur WireGuard

Le serveur VPN doit autoriser l’adresse locale choisie pour la carte, par exemple 10.0.0.2, et connaître la clé publique du pair MeshCore.

Récapitulatif minimal :

1. créer un peer WireGuard pour la carte,
2. associer la clé publique de la carte,
3. autoriser l’IP VPN locale de la carte dans la configuration du serveur,
4. ouvrir le port UDP du serveur, en général 51820.

## 7. Référence rapide

Les points d’entrée utiles dans le dépôt sont :

- [variants/xiao_s3_wio/platformio.ini](../variants/xiao_s3_wio/platformio.ini)
- [variants/xiao_s3_wio/WireGuardSettings.h](../variants/xiao_s3_wio/WireGuardSettings.h)
- [src/helpers/esp32/WireGuardConfig.h](../src/helpers/esp32/WireGuardConfig.h)
- [src/helpers/esp32/WireGuardManager.h](../src/helpers/esp32/WireGuardManager.h)
- [src/helpers/esp32/WireGuardSerialInterface.h](../src/helpers/esp32/WireGuardSerialInterface.h)

Si tu veux, le prochain pas logique est de faire une version plus courte de ce guide dans la FAQ, avec seulement la commande PlatformIO et les champs à remplir.