//
//  Ejercicio3.cpp
//  Algoritmo BT (Binary Tree / Arbol Binario) para el calculo de todos los
//  testores tipicos de una matriz basica.
//
//  Convencion: cada lista alfa = (alfa_1,...,alfa_n) se guarda como un entero
//  sin signo donde x1 es el bit MAS significativo y xn el MENOS significativo.
//  Asi, el orden numerico de los enteros coincide con el "orden natural" del
//  material de BT.   Ejemplo (n = 5):  (0,1,1,0,0) = 01100 = 12 = {x2,x3}.
//

#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <cstdint>
#include <chrono>
#include <iomanip>

using namespace std;

using Mascara = uint64_t;

// ============================================================================
// REPRESENTACION BINARIA
// ============================================================================

// Convierte una fila de 0/1 en su mascara (x1 = bit mas significativo)
Mascara filaAMascara(const vector<int>& fila) {
    Mascara m = 0;
    for (int v : fila) m = (m << 1) | static_cast<Mascara>(v);
    return m;
}

// Mascara -> cadena "01100"
string mascaraACadena(Mascara m, int n) {
    string s(n, '0');
    for (int j = 0; j < n; ++j)
        if ((m >> (n - 1 - j)) & 1ULL) s[j] = '1';
    return s;
}

// Mascara -> conjunto "{x2,x3}"
string mascaraAConjunto(Mascara m, int n) {
    string s = "{";
    bool primero = true;
    for (int j = 0; j < n; ++j) {
        if ((m >> (n - 1 - j)) & 1ULL) {
            if (!primero) s += ",";
            s += "x" + to_string(j + 1);
            primero = false;
        }
    }
    return s + "}";
}

// Posicion k (1..n, de izquierda a derecha) del ULTIMO 1 de la mascara.
// El ultimo 1 es el bit encendido menos significativo.
int posicionUltimoUno(Mascara m, int n) {
    int p = __builtin_ctzll(m);   // indice del bit menos significativo encendido
    return n - p;
}

// ============================================================================
// PROPOSICION 1.3: caracterizacion de testor
// alfa NO es testor <=> existe una fila a de MB con (alfa AND a) = 0
// ============================================================================
bool esTestor(Mascara alfa, const vector<Mascara>& MB) {
    for (Mascara a : MB)
        if ((alfa & a) == 0) return false;
    return true;
}

// Fila de MB que deja a alfa sin cubrir. Si hay varias se toma la que tiene el
// ULTIMO 1 MAS A LA IZQUIERDA (mayor salto). Devuelve el indice, o -1 si alfa
// es testor.
int filaResponsable(Mascara alfa, const vector<Mascara>& MB) {
    int mejor = -1;
    int mejorK = numeric_limits<int>::max();
    for (size_t i = 0; i < MB.size(); ++i) {
        if ((alfa & MB[i]) == 0) {
            int p = __builtin_ctzll(MB[i]);
            // mayor p  <=>  menor k  <=>  ultimo 1 mas a la izquierda
            int k = -p;
            if (mejor == -1 || k < mejorK) {
                mejor = static_cast<int>(i);
                mejorK = k;
            }
        }
    }
    return mejor;
}

// ============================================================================
// TIPICIDAD
// ============================================================================

// Forma 2 del material: se comprueba directamente que al quitar cualquier
// columna de alfa deja de ser testor.
bool esTipicoDirecto(Mascara alfa, const vector<Mascara>& MB) {
    Mascara resto = alfa;
    while (resto) {
        Mascara bit = resto & (~resto + 1);   // bit menos significativo
        if (esTestor(alfa ^ bit, MB)) return false;
        resto ^= bit;
    }
    return true;
}

// Forma 1 del material: alfa (testor) se almacena si no hay almacenado un
// testor contenido en alfa; en ese caso se eliminan los almacenados que
// contengan a alfa.
bool tipicoPorAlmacenados(Mascara alfa, vector<Mascara>& tipicos) {
    for (Mascara t : tipicos)
        if ((t & alfa) == t) return false;     // t subconjunto de alfa
    tipicos.erase(remove_if(tipicos.begin(), tipicos.end(),
                            [&](Mascara t) { return (alfa & t) == alfa; }),
                  tipicos.end());
    tipicos.push_back(alfa);
    return true;
}

// ============================================================================
// ALGORITMO BT
// ============================================================================
struct Paso {
    Mascara alfa;
    bool testor;
    int fila;            // indice (0-based) de la fila responsable, -1 si es testor
    int k;
    Mascara siguiente;
    bool tipico;
    vector<Mascara> guardados;
};

struct ResultadoBT {
    vector<Mascara> tipicos;
    vector<Mascara> testores;     // todos los testores examinados por BT
    long long examinadas = 0;
    vector<Paso> traza;
};

ResultadoBT ejecutarBT(const vector<Mascara>& MB, int n, bool guardarTraza) {
    ResultadoBT r;
    const Mascara ultimo = (n == 64) ? ~0ULL : ((1ULL << n) - 1);
    Mascara alfa = 1;                      // Paso 1: (0,...,0,1)

    while (alfa <= ultimo) {               // Paso 4: parar al pasar de (1,...,1)
        ++r.examinadas;
        int fi = filaResponsable(alfa, MB);   // Paso 2: proposicion 1.3

        if (fi == -1) {                    // ---- alfa es testor ----
            r.testores.push_back(alfa);
            int k = posicionUltimoUno(alfa, n);

            // Paso 3: tipicidad. Si hay menos testores almacenados que filas
            // en MB se usa la forma 1; en otro caso, la forma 2.
            bool tipico;
            if (r.tipicos.size() < MB.size()) {
                tipico = tipicoPorAlmacenados(alfa, r.tipicos);
            } else {
                tipico = esTipicoDirecto(alfa, MB);
                if (tipico) r.tipicos.push_back(alfa);
            }

            Mascara sig = alfa + (1ULL << (n - k));   // proposicion 1.4
            if (guardarTraza)
                r.traza.push_back({alfa, true, -1, k, sig, tipico, r.tipicos});
            alfa = sig;
        } else {                           // ---- alfa NO es testor ----
            int k = posicionUltimoUno(MB[fi], n);
            int p = n - k;
            // proposicion 1.5: se conserva alfa antes de k, se pone 1 en k
            // y ceros despues
            Mascara sig = ((alfa >> (p + 1)) << (p + 1)) | (1ULL << p);
            if (guardarTraza)
                r.traza.push_back({alfa, false, fi, k, sig, false, r.tipicos});
            alfa = sig;
        }
    }
    sort(r.tipicos.begin(), r.tipicos.end());
    return r;
}

// ============================================================================
// SALIDA
// ============================================================================
void imprimirTraza(const ResultadoBT& r, int n, ostream& csv) {
    cout << left << setw(6) << "Paso" << setw(n + 3) << "alfa" << setw(15)
         << "Testor?" << setw(8) << "Fila" << setw(4) << "k" << setw(n + 3)
         << "Salto a" << "Tipicos guardados\n";
    cout << string(60 + n * 2, '-') << "\n";
    csv << "paso,alfa,testor,fila_responsable,k,salto_a,tipicos_guardados\n";
    int paso = 1;
    for (const Paso& s : r.traza) {
        string et = s.testor ? (s.tipico ? "Si (tipico)" : "Si (no tipico)") : "No";
        string fila = s.fila < 0 ? "-" : "F" + to_string(s.fila + 1);
        bool fin = s.siguiente >= (1ULL << n);
        string sig = fin ? "FIN" : mascaraACadena(s.siguiente, n);
        string g;
        for (size_t i = 0; i < s.guardados.size(); ++i) {
            if (i) g += " ";
            g += mascaraAConjunto(s.guardados[i], n);
        }
        if (g.empty()) g = "-";
        cout << left << setw(6) << paso << setw(n + 3) << mascaraACadena(s.alfa, n)
             << setw(15) << et << setw(8) << fila << setw(4) << s.k
             << setw(n + 3) << sig << g << "\n";
        csv << paso << ',' << mascaraACadena(s.alfa, n) << ',' << (s.testor ? 1 : 0)
            << ',' << fila << ',' << s.k << ',' << sig << ",\"" << g << "\"\n";
        ++paso;
    }
}

// ============================================================================
// ENTRADA Y VALIDACION (misma logica que el Ejercicio 2)
// ============================================================================
bool leerMatriz(istream& entrada, vector<vector<int>>& matriz) {
    string linea;
    if (!getline(entrada, linea)) return false;
    istringstream encabezado(linea);
    int filas, columnas;
    if (!(encabezado >> filas >> columnas) || filas <= 0 || columnas <= 0 ||
        (encabezado >> ws).peek() != char_traits<char>::eof()) {
        return false;
    }
    for (int fila = 0; fila < filas; ++fila) {
        if (!getline(entrada, linea)) return false;
        istringstream datos(linea);
        vector<int> valores;
        for (int col = 0; col < columnas; ++col) {
            string bit;
            if (!(datos >> bit) || (bit != "0" && bit != "1")) return false;
            valores.push_back(bit == "1");
        }
        if ((datos >> ws).peek() != char_traits<char>::eof()) return false;
        matriz.push_back(std::move(valores));
    }
    return (entrada >> ws).eof() && !entrada.bad();
}

bool esMatrizBasica(const vector<vector<int>>& matriz) {
    if (matriz.empty() || matriz.front().empty()) return false;
    const auto incluido = [](int a, int b) { return a <= b; };
    for (size_t i = 0; i < matriz.size(); ++i) {
        const auto& fila = matriz[i];
        if (none_of(fila.begin(), fila.end(), [](int v) { return v == 1; })) return false;
        for (size_t j = 0; j < i; ++j) {
            const auto& anterior = matriz[j];
            if (equal(fila.begin(), fila.end(), anterior.begin(), incluido) ||
                equal(anterior.begin(), anterior.end(), fila.begin(), incluido))
                return false;
        }
    }
    return true;
}

int main() {
    cout << "====================================================================\n";
    cout << "       IMPLEMENTACION COMPUTACIONAL DEL ALGORITMO BT (EJ. 3.2)\n";
    cout << "====================================================================\n\n";
    cout << "1. Usar la Matriz Basica A\n";
    cout << "2. Ingresar una Matriz Basica personalizada\n";
    cout << "3. Leer una Matriz Basica desde un archivo de texto\n";
    cout << "4. Usar la Matriz B\n";
    cout << "Opcion: ";
    int opcion;
    if (!(cin >> opcion) || opcion < 1 || opcion > 4) {
        cerr << "Opcion invalida. Elija 1, 2, 3 o 4.\n";
        return 1;
    }

    vector<vector<int>> BM;
    if (opcion == 1) {
        BM = {
            {1, 0, 0, 0, 0, 1, 1},
            {1, 0, 1, 0, 0, 0, 0},
            {0, 0, 1, 0, 1, 1, 0},
            {0, 1, 0, 1, 0, 0, 1},
            {1, 1, 0, 0, 0, 0, 1},
            {0, 0, 1, 1, 0, 1, 0},
            {0, 1, 1, 0, 0, 0, 1}
        };
        cout << "\n-> Matriz Basica A cargada (7 filas x 7 columnas).\n";
    } else if (opcion == 4) {
        BM = {
            {0, 0, 1, 0, 0, 0, 1},
            {1, 1, 1, 0, 0, 0, 0},
            {1, 0, 1, 1, 0, 0, 0},
            {1, 1, 0, 0, 0, 1, 0},
            {1, 0, 0, 0, 0, 1, 1},
            {0, 1, 1, 0, 0, 1, 0},
            {1, 1, 0, 1, 1, 0, 0},
            {1, 0, 0, 1, 1, 0, 1}
        };
        cout << "\n-> Matriz B cargada (8 filas x 7 columnas).\n";
    } else if (opcion == 2) {
        int filas, columnas;
        cout << "Ingrese el numero de filas: ";
        if (!(cin >> filas) || filas <= 0) {
            cerr << "El numero de filas debe ser un entero positivo.\n";
            return 1;
        }
        cout << "Ingrese el numero de columnas (maximo 62): ";
        if (!(cin >> columnas) || columnas <= 0 || columnas > 62) {
            cerr << "El numero de columnas debe estar entre 1 y 62.\n";
            return 1;
        }
        cout << "Ingrese los elementos de la matriz (0 y 1):\n";
        for (int f = 0; f < filas; ++f) {
            vector<int> valores;
            for (int c = 0; c < columnas; ++c) {
                string bit;
                if (!(cin >> bit) || (bit != "0" && bit != "1")) {
                    cerr << "Cada elemento debe ser 0 o 1.\n";
                    return 1;
                }
                valores.push_back(bit == "1");
            }
            BM.push_back(std::move(valores));
        }
    } else {
        cout << "Ruta del archivo (Enter para matriz_basica.txt): ";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        string ruta;
        getline(cin, ruta);
        if (ruta.empty()) ruta = "matriz_basica.txt";
        ifstream entrada(ruta);
        if (!entrada || !leerMatriz(entrada, BM)) {
            cerr << "No se pudo leer una matriz booleana en " << ruta << ".\n";
            return 1;
        }
    }

    if (!esMatrizBasica(BM)) {
        cerr << "Se requiere una matriz basica no vacia, sin filas nulas, "
             << "duplicadas ni superfilas.\n";
        return 1;
    }
    const int n = static_cast<int>(BM[0].size());
    if (n > 62) {
        cerr << "Esta implementacion admite hasta 62 columnas.\n";
        return 1;
    }

    vector<Mascara> MB;
    for (const auto& f : BM) MB.push_back(filaAMascara(f));

    // --- Medicion: solo el algoritmo, sin traza ni impresion ---------------
    using Reloj = chrono::steady_clock;
    const int repeticiones = 1000;   // BT es muy rapido: se promedian corridas
    auto inicio = Reloj::now();
    ResultadoBT medido;
    for (int i = 0; i < repeticiones; ++i) medido = ejecutarBT(MB, n, false);
    double totalMs = chrono::duration<double, milli>(Reloj::now() - inicio).count();
    double promedioMs = totalMs / repeticiones;

    // Una segunda corrida, fuera del cronometro, para registrar la traza
    ResultadoBT r = ejecutarBT(MB, n, true);

    cout << "\nMatriz basica (" << MB.size() << " filas x " << n << " columnas):\n";
    cout << "      ";
    for (int j = 1; j <= n; ++j) cout << "x" << j << " ";
    cout << "\n";
    for (size_t i = 0; i < MB.size(); ++i) {
        cout << "F" << i + 1 << "    ";
        for (int j = 0; j < n; ++j) cout << ((MB[i] >> (n - 1 - j)) & 1ULL) << "  ";
        cout << "\n";
    }

    cout << "\n--- RECORRIDO DEL ARBOL BINARIO (solo listas examinadas) ---\n\n";
    ofstream csv("bt_resultados.csv");
    imprimirTraza(r, n, csv);

    cout << "\nListas examinadas: " << r.examinadas << " de "
         << ((1ULL << n) - 1) << " posibles ("
         << fixed << setprecision(2)
         << 100.0 * r.examinadas / ((1ULL << n) - 1) << "%)\n";

    cout << "\nTestores encontrados por BT (" << r.testores.size() << "):\n";
    for (Mascara t : r.testores)
        cout << "   " << mascaraACadena(t, n) << "  " << mascaraAConjunto(t, n) << "\n";

    cout << "\nTestores TIPICOS (" << r.tipicos.size() << "):\n";
    for (Mascara t : r.tipicos)
        cout << "   " << mascaraACadena(t, n) << "  " << mascaraAConjunto(t, n) << "\n";

    cout << "\nTiempo total de ejecucion de BT (promedio de " << repeticiones
         << " corridas): " << fixed << setprecision(6) << promedioMs << " ms\n";

    csv.close();
    cout << "Traza guardada en bt_resultados.csv.\n";
    return 0;
}
