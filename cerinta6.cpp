#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SHARED_MAP_NAME L"SharedCounterMap"
#define SEMAPHORE_NAME L"CounterSemaphore"
#define MAX_LIMIT 1000

typedef struct {
    int current_number; // Numărul curent de la 1 la MAX_LIMIT
    int max_limit;      // Limita maximă (1000)
} SharedData;

// Functie principala de logica a procesului
void RunProcessLogic(HANDLE hSemaphore, SharedData* pData) {
    srand((unsigned int)time(NULL) + GetCurrentProcessId()); // Seed-ul pentru random

    while (TRUE) {
        // 1. REZERVĂ (Așteaptă semaforul)
        DWORD waitResult = WaitForSingleObject(hSemaphore, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            // Accesul la memorie este protejat

            // 2. CITEȘTE MEMORIA
            if (pData->current_number >= pData->max_limit) {
                // S-a terminat numărătoarea
                printf("[%lu] Toate numerele au fost atinse. Iesire.\n", GetCurrentProcessId());
                ReleaseSemaphore(hSemaphore, 1, NULL); // Eliberează semaforul înainte de a ieși
                break;
            }

            int current = pData->current_number;
            printf("\n[%lu] Procesul a preluat semaforul. Valoare curenta: %d.\n", 
                   GetCurrentProcessId(), current);

            // 3. DAU CU BANUL (random(2))
            // rand() % 2 returnează 0 sau 1. Presupunem că 1 este "cade 2"
            int coin_result = rand() % 2; 
            
            while (coin_result == 1) { 
                current++;
                if (current > pData->max_limit) {
                    break;
                }
                
                // SCRIU ÎN MEMORIE URMĂTORUL NUMĂR
                pData->current_number = current;
                printf("[%lu] *Scriere* Numar nou: %d\n", GetCurrentProcessId(), current);

                Sleep(10); // Pauză mică pentru a simula timpul de lucru/IO

                coin_result = rand() % 2; // Aruncă din nou
            }

            printf("[%lu] 'Dau cu banul' s-a oprit pe 0. Valoare finala scrisa: %d.\n", 
                   GetCurrentProcessId(), pData->current_number);

            // 4. ELIBEREAZĂ SEMAFORUL
            if (!ReleaseSemaphore(hSemaphore, 1, NULL)) {
                fprintf(stderr, "[%lu] EROARE la ReleaseSemaphore: %lu\n", 
                        GetCurrentProcessId(), GetLastError());
            }
        } else {
            fprintf(stderr, "[%lu] EROARE la WaitForSingleObject: %lu\n", 
                    GetCurrentProcessId(), waitResult);
            break;
        }

        Sleep(100 + rand() % 100); // Pauză între încercările de a lua semaforul
    }
}

// Functia Main
int wmain(int argc, wchar_t *argv[]) {
    BOOL is_server = (argc > 1 && wcscmp(argv[1], L"server") == 0);
    HANDLE hMapFile = NULL;
    HANDLE hSemaphore = NULL;
    SharedData* pData = NULL;

    printf("--- Procesul %s (PID: %lu) a inceput ---\n", 
           is_server ? "SERVER" : "CLIENT", GetCurrentProcessId());

    // 1. INITIALIZARE SEMAFOR (Server creaza, Client deschide)
    if (is_server) {
        // SERVER: Crează semaforul (Initial Count=1, Max Count=1)
        hSemaphore = CreateSemaphoreW(
            NULL,               // Atribute de securitate default
            1,                  // Contor initial (liber)
            1,                  // Contor maxim (mutex logic)
            SEMAPHORE_NAME      // Nume
        );
        if (hSemaphore == NULL) {
            fprintf(stderr, "SERVER: EROARE la CreateSemaphore: %lu\n", GetLastError());
            return 1;
        }
        printf("SERVER: Semaforul '%ls' creat cu succes.\n", SEMAPHORE_NAME);
    } else {
        // CLIENT: Deschide semaforul existent
        hSemaphore = OpenSemaphoreW(
            SEMAPHORE_ALL_ACCESS, // Drepturi de acces
            FALSE,                // Nu se moștenește
            SEMAPHORE_NAME        // Nume
        );
        if (hSemaphore == NULL) {
            fprintf(stderr, "CLIENT: EROARE la OpenSemaphore (asteptati sa porneasca serverul): %lu\n", GetLastError());
            return 1;
        }
        printf("CLIENT: Semaforul '%ls' deschis cu succes.\n", SEMAPHORE_NAME);
    }

    // 2. INITIALIZARE MEMORIE PARTAJATĂ (Server creaza, Client deschide)
    if (is_server) {
        // SERVER: Creaza Obiectul Mapat in Memorie
        hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE, // Folosește memoria de paginare
            NULL,                 // Atribute de securitate default
            PAGE_READWRITE,       // Permisiuni R/W
            0, sizeof(SharedData),// Dimensiune
            SHARED_MAP_NAME       // Nume
        );
        if (hMapFile == NULL) {
            fprintf(stderr, "SERVER: EROARE la CreateFileMapping: %lu\n", GetLastError());
            CloseHandle(hSemaphore);
            return 1;
        }

        // Mapează vederea in spațiul de adrese al procesului
        pData = (SharedData*)MapViewOfFile(
            hMapFile,             // Handle-ul obiectului mapat
            FILE_MAP_ALL_ACCESS,  // Acces R/W
            0, 0, sizeof(SharedData)
        );
        if (pData == NULL) {
            fprintf(stderr, "SERVER: EROARE la MapViewOfFile: %lu\n", GetLastError());
            CloseHandle(hMapFile);
            CloseHandle(hSemaphore);
            return 1;
        }

        // INITIALIZARE DATE
        pData->current_number = 0; // Începe de la 0, procesele vor număra de la 1
        pData->max_limit = MAX_LIMIT;
        printf("SERVER: Memorie mapata '%ls' initializata (limit=%d).\n", 
               SHARED_MAP_NAME, pData->max_limit);

    } else {
        // CLIENT: Deschide Obiectul Mapat existent
        hMapFile = OpenFileMappingW(
            FILE_MAP_ALL_ACCESS, // Drepturi de acces
            FALSE,               // Nu se moștenește
            SHARED_MAP_NAME
        );
        if (hMapFile == NULL) {
            fprintf(stderr, "CLIENT: EROARE la OpenFileMapping: %lu\n", GetLastError());
            CloseHandle(hSemaphore);
            return 1;
        }

        // Mapează vederea in spațiul de adrese al procesului
        pData = (SharedData*)MapViewOfFile(
            hMapFile,             // Handle-ul obiectului mapat
            FILE_MAP_ALL_ACCESS,  // Acces R/W
            0, 0, sizeof(SharedData)
        );
        if (pData == NULL) {
            fprintf(stderr, "CLIENT: EROARE la MapViewOfFile: %lu\n", GetLastError());
            CloseHandle(hMapFile);
            CloseHandle(hSemaphore);
            return 1;
        }
        printf("CLIENT: Memorie mapata '%ls' deschisa cu succes.\n", SHARED_MAP_NAME);
    }

    // 3. RULAREA LOGICII PRINCIPALE
    RunProcessLogic(hSemaphore, pData);

    // 4. CURĂȚAREA RESURSELOR
    // Serverul ar trebui să ruleze ultimul pentru a nu închide obiectele
    UnmapViewOfFile(pData);
    CloseHandle(hMapFile);
    CloseHandle(hSemaphore);

    printf("--- Procesul %s (PID: %lu) a terminat ---\n", 
           is_server ? "SERVER" : "CLIENT", GetCurrentProcessId());

    return 0;
}