# README : Implémentation de `malloc` dans le Kernel WeensyOS

## Introduction

Ce document décrit l'implémentation d'un allocateur de mémoire sécurisé pour le kernel WeensyOS, inspiré par les mécanismes de gestion de mémoire sécurisée des systèmes embarqués comme ceux des iPhones. L'allocateur est implémenté dans le fichier `k-malloc.c` et fournit les fonctions `kernel_malloc`, `kernel_free`, et `testmalloc`. L'objectif est de gérer un tas (heap) dans le kernel avec des considérations de sécurité, telles que l'effacement de la mémoire et l'alignement pour une compatibilité avec le chiffrement.

## Architecture de l'Allocateur

### Structure du Tas
- **Pages du Tas** : Le tas est constitué de pages de 4 Ko allouées via `page_alloc(PO_KERNEL_HEAP)`. Ces pages sont suivies dans le tableau `heap_pageinfo` (jusqu'à `HEAP_MAXPAGES = 1024` pages).
- **Blocs de Mémoire** : Chaque page est divisée en blocs, chacun précédé d'un en-tête (`block_header`) contenant :
  - `size` : Taille du bloc (hors en-tête).
  - `next` : Pointeur vers le bloc libre suivant (pour les blocs libres uniquement).
- **Liste Libre** : Les blocs libres sont organisés dans une liste chaînée simple (`free_list`), utilisant une stratégie de recherche "premier adapté" (first-fit).
- **Alignement** : Les allocations sont alignées sur 16 octets pour supporter les algorithmes de chiffrement (comme AES) et les opérations SIMD.

### Considérations de Sécurité
- **Effacement de la Mémoire** : Toute mémoire allouée ou libérée est mise à zéro pour prévenir les fuites de données, conformément aux pratiques des systèmes sécurisés comme iOS.
- **Alignement des Pointeurs** : Les pointeurs retournés par `kernel_malloc` sont alignés sur 16 octets, facilitant une intégration future avec un chiffrement au niveau des blocs.
- **Validation** : Les tailles d'allocation et les pointeurs libérés sont validés pour éviter les erreurs comme les doubles libérations ou les débordements de mémoire.
- **Compatibilité avec le Chiffrement** : Bien que le chiffrement (par exemple, AES) ne soit pas implémenté en raison de l'absence de primitives cryptographiques dans le kernel fourni, l'allocateur est conçu pour être compatible avec une couche de chiffrement en alignant les blocs et en effaçant la mémoire.

## Fonctions Principales

### `extend_heap`
- **Rôle** : Étend le tas en allouant une nouvelle page de 4 Ko.
- **Fonctionnement** :
  - Vérifie si la limite de `HEAP_MAXPAGES` est atteinte.
  - Alloue une page via `page_alloc(PO_KERNEL_HEAP)`.
  - Met à zéro la page pour des raisons de sécurité.
  - Mappe la page dans la table de pages du kernel avec les permissions `PTE_P | PTE_W`.
  - Ajoute la page au tableau `heap_pageinfo` et incrémente `heap_pagecount`.
  - Initialise la page comme un seul bloc libre ajouté à `free_list`.
- **Sécurité** : L'effacement de la mémoire empêche les fuites de données sensibles.

### `kernel_malloc`
- **Rôle** : Alloue un bloc de mémoire de la taille demandée.
- **Fonctionnement** :
  - Valide la taille demandée (non nulle et inférieure à `PAGESIZE - sizeof(block_header)`).
  - Aligne la taille (incluant l'en-tête) sur 16 octets.
  - Recherche un bloc libre suffisamment grand dans `free_list` (stratégie first-fit).
  - Si un bloc est trouvé :
    - Divise le bloc si l'espace restant est suffisant pour un autre bloc (au moins `MIN_ALLOC_SIZE + sizeof(block_header)`).
    - Retire le bloc de la liste libre.
    - Met à zéro la partie données du bloc pour la sécurité.
    - Retourne un pointeur vers la zone de données (après l'en-tête).
  - Si aucun bloc n'est trouvé, appelle `extend_heap` et réessaye.
- **Sécurité** : Alignement et effacement de la mémoire pour prévenir les fuites et assurer la compatibilité avec le chiffrement.

### `kernel_free`
- **Rôle** : Libère un bloc de mémoire alloué.
- **Fonctionnement** :
  - Valide le pointeur en vérifiant s'il appartient à une page du tas et s'il est marqué comme alloué (`next == NULL`).
  - Met à zéro les données du bloc pour la sécurité.
  - Ajoute le bloc à `free_list`.
  - Fusionne les blocs libres adjacents pour réduire la fragmentation.
- **Sécurité** : La validation du pointeur empêche les libérations invalides, et l'effacement de la mémoire protège contre les fuites.

### `testmalloc`
- **Rôle** : Teste l'implémentation de l'allocateur avec divers scénarios.
- **Fonctionnement** :
  - Exécute plusieurs tests :
    1. Allocation et libération de 100 octets.
    2. Allocations multiples (200 et 300 octets) suivies de libérations.
    3. Allocation d'un grand bloc (2048 octets) et libération.
    4. Test de stress avec 10 allocations de 50 octets chacune, suivies de libérations.
    5. Test d'allocation de taille nulle (doit échouer).
    6. Allocation d'une taille spécifiée par un argument (si fourni).
  - Enregistre les opérations dans `log.txt` pour le débogage.
- **Sécurité** : Vérifie les cas limites pour s'assurer que l'allocateur est robuste.

## Intégration avec WeensyOS
- **Allocation de Pages** : Utilise `page_alloc(PO_KERNEL_HEAP)` pour obtenir des pages, suivies dans `pageinfo` avec l'identifiant `PO_KERNEL_HEAP`.
- **Mappage de Mémoire** : Les pages du tas sont mappées dans la table de pages du kernel avec `virtual_memory_map` et les permissions `PTE_P | PTE_W`.
- **Appel via System Call** : La fonction `testmalloc` peut être invoquée via l'appel système `INT_SYS_EXECV` avec la commande `testmalloc`.

## Test de l'Implémentation

### Exécution Automatique
Pour tester automatiquement l'allocateur lors du démarrage du kernel :
1. **Créer `p-testmalloc.c`** :
   - Créez un fichier `p-testmalloc.c` avec le code suivant :
     ```c
     #include "lib.h"
     int main(int argc, char** argv) {
         testmalloc(argc > 1 ? argv[1] : NULL);
         syscall_exit(0);
         return 0;
     }
     ```
   - Placez-le dans le répertoire des programmes utilisateurs (par exemple, `p-`).
2. **Modifier `kernel.c`** :
   - Dans la fonction `kernel`, ajoutez :
     ```c
     process_setup(6, 9, 0); // PID 6, programme 9 (testmalloc)
     run(&processes[6]);     // Démarrer avec testmalloc
     ```
   - Dans `exception`, sous `INT_SYS_EXECV`, ajoutez :
     ```c
     } else if (strcmp(path, "testmalloc") == 0) {
         log_printf("run testmalloc\n");
         program_number = 9;
     }
     ```
3. **Mettre à Jour le Makefile** :
   - Ajoutez `p-testmalloc.o` à la liste des objets utilisateurs :
     ```makefile
     UOBS = p-allocator.o p-fork.o p-hello.o p-cat.o p-echo.o p-ls.o p-mkdir.o p-rand.o p-entropy.o p-testmalloc.o
     ```
   - Ajoutez une règle pour compiler `p-testmalloc.c` :
     ```makefile
     obj/p-testmalloc.o: p-testmalloc.c
         $(CC) $(CFLAGS) -c -o $@ $<
     ```
   - Mettez à jour `ramimages` (par exemple, dans `ramimages.S`) pour inclure :
     ```assembly
     .quad _binary_obj_p_testmalloc_start, _binary_obj_p_testmalloc_end, 9
     ```
4. **Compiler et Exécuter** :
   - Exécutez `make clean && make && make run` pour compiler et lancer QEMU.
   - Vérifiez `log.txt` pour les sorties de `testmalloc` (allocations, libérations, messages de test).
   - Observez la console CGA pour les cartes mémoire, montrant les pages du tas (`H`).

### Vérifications
- **Sortie dans `log.txt`** : Attendez-vous à des messages comme :
  ```
  run testmalloc
  testmalloc(NULL)
  testmalloc: Test run 1
  extend_heap: Allocated page at 0x...
  kernel_malloc: Allocated 100 bytes at 0x...
  kernel_free: Freed 100 bytes at 0x...
  testmalloc: Test complete
  ```
- **Carte Mémoire** : Vérifiez que les pages du tas (`H`) apparaissent dans la carte mémoire physique.
- **Tests de Sécurité** :
  - Ajoutez des tests dans `p-testmalloc.c` pour vérifier l'effacement de la mémoire :
    ```c
    void* p = kernel_malloc(100);
    memset(p, 0xFF, 100);
    kernel_free(p);
    p = kernel_malloc(100);
    for (int i = 0; i < 100; i++) {
        if (((char*)p)[i] != 0) {
            console_printf(CPOS(24, 0), 0x0C00, "Mémoire non effacée à %p\n", p);
            syscall_exit(1);
        }
    }
    console_printf(CPOS(24, 0), 0x0F00, "Effacement de la mémoire vérifié\n");
    ```
  - Vérifiez l'alignement des pointeurs :
    ```c
    void* p = kernel_malloc(100);
    if ((uintptr_t)p % 16 != 0) {
        console_printf(CPOS(24, 0), 0x0C00, "Pointeur non aligné %p\n", p);
        syscall_exit(1);
    }
    console_printf(CPOS(24, 0), 0x0F00, "Alignement vérifié\n");
    ```

## Dépannage
- **Pas de Sortie** : Vérifiez que `p-testmalloc` est correctement compilé et mappé au numéro de programme 9.
- **Plantages** : Si le kernel panique (par exemple, `INT_PAGEFAULT`), vérifiez les adresses dans `log.txt`. Assurez-vous que `virtual_memory_map` dans `extend_heap` utilise les bonnes permissions.
- **Fuites de Mémoire** : Surveillez `heap_pagecount` et `free_list` pour confirmer la réutilisation des blocs.
- **Erreurs de Programme** : Si `testmalloc` ne s'exécute pas, vérifiez les entrées dans `ramimages` et le mappage dans `INT_SYS_EXECV`.

## Conclusion
Cette implémentation de `malloc` dans le kernel WeensyOS fournit un allocateur robuste et sécurisé, adapté aux systèmes embarqués. Les tests automatisés via `p-testmalloc` permettent de valider son fonctionnement, et les fonctionnalités de sécurité (effacement de la mémoire, alignement) le rendent compatible avec des exigences de chiffrement. Pour des améliorations futures, une intégration avec une API de chiffrement (par exemple, AES) pourrait être ajoutée si le kernel fournit des primitives cryptographiques.