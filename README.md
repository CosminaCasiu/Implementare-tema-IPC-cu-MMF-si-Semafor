# Implementare-tema-IPC-cu-MMF-si-Semafor

Scopul : Implementarea unei numărători concurente de la 1 la 1000 între două sau mai multe procese independente pe sistemul de operare Windows. Proiectul demonstrează Comunicația Inter-Proces (IPC) folosind Memory-Mapped Files (MMF) și Sincronizarea prin Semafoare.

Componente Cheie
Memorie Partajată: Un obiect Memory-Mapped File numit SharedCounterMap găzduiește structura SharedData, care conține variabila current_number.

Sincronizare: Un Semafor numit CounterSemaphore protejează accesul la current_number, prevenind Condițiile de Cursă (Race Conditions).

Logica de Numărare: Procesul care deține semaforul execută logica "Dau cu banul" (while (rand() % 2 == 1)), scriind o serie de numere consecutive înainte de a elibera controlul.
