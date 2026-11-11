//
//  Ejercicio4.cpp
//  Ejercicio 4.2: uso del algoritmo BT para determinar todos los testores
//  tipicos de la matriz B, cuyas filas son los testores tipicos de A (4.1).
//
//  Flujo del programa:
//    1. Se obtiene la matriz basica A.
//    2. Se calculan con BT los testores tipicos de A y se construye B
//       escribiendo cada testor tipico en notacion binaria (Ejercicio 4.1).
//    3. Se verifica que B es una matriz basica.
//    4. Se ejecuta BT sobre B: recorrido, testores, testores tipicos y tiempo.
//    5. Se comparan los testores tipicos de B con las filas de A (apoyo al 4.3).
//
//  El algoritmo BT es el mismo de Ejercicio3.cpp (Algo BT.pdf):
//  cada lista alfa = (alfa_1,...,alfa_n) se guarda como un entero sin signo
//  donde x1 es el bit MAS significativo, de modo que el orden numerico
//  coincide con el "orden natural" del documento.
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

Mascara filaAMascara(const vector<int>& fila) {
    Mascara m = 0;
    for (int v : fila) m = (m << 1) | static_cast<Mascara>(v);
    return m;
}

vector<int> mascaraAFila(Mascara m, int n) {
    vector<int> fila(n);
    for (int j = 0; j < n; ++j) fila[j] = static_cast<int>((m >> (n - 1 - j)) & 1ULL);
    return fila;
}

string mascaraACadena(Mascara m, int n) {
    string s(n, '0');
    for (int j = 0; j < n; ++j)
        if ((m >> (n - 1 - j)) & 1ULL) s[j] = '1';
    return s;
}

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
int posicionUltimoUno(Mascara m, int n) {
    return n - __builtin_ctzll(m);
}

// ============================================================================
// PROPOSICION 1.3: alfa NO es testor <=> existe fila a de MB con alfa AND a = 0
// ============================================================================
bool esTestor(Mascara alfa, const vector<Mascara>& MB) {
    for (Mascara a : MB)
        if ((alfa & a) == 0) return false;
    return true;
}

// Fila de MB que deja a alfa sin cubrir; si hay varias, la que tiene el
// ULTIMO 1 MAS A LA IZQUIERDA (mayor salto). Devuelve -1 si alfa es testor.
int filaResponsable(Mascara alfa, const vector<Mascara>& MB) {
    int mejor = -1;
    int mejorP = -1;
    for (size_t i = 0; i < MB.size(); ++i) {
        if ((alfa & MB[i]) == 0) {
            int p = __builtin_ctzll(MB[i]);   // mayor p <=> ultimo 1 mas a la izquierda
            if (p > mejorP) {
                mejor = static_cast<int>(i);
                mejorP = p;
            }
        }
    }
    return mejor;
}

// ============================================================================
// TIPICIDAD (paso 3 del documento)
// ============================================================================

// Forma 2: al quitar cualquier columna de alfa deja de ser testor.
bool esTipicoDirecto(Mascara alfa, const vector<Mascara>& MB) {
    Mascara resto = alfa;
    while (resto) {
        Mascara bit = resto & (~resto + 1);
        if (esTestor(alfa ^ bit, MB)) return false;
        resto ^= bit;
    }
    return true;
}

// Forma 1: alfa se almacena si no hay almacenado un testor contenido en alfa;
// en ese caso se eliminan los almacenados que contengan a alfa.
bool tipicoPorAlmacenados(Mascara alfa, vector<Mascara>& tipicos) {
    for (Mascara t : tipicos)
        if ((t & alfa) == t) return false;
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
    int fila;
    int k;
    Mascara siguiente;
    bool tipico;
    vector<Mascara> guardados;
};

struct ResultadoBT {
    vector<Mascara> tipicos;
    vector<Mascara> testores;
    long long examinadas = 0;
    vector<Paso> traza;
};

ResultadoBT ejecutarBT(const vector<Mascara>& MB, int n, bool guardarTraza) {
    ResultadoBT r;
    const Mascara ultimo = (1ULL << n) - 1;   // n <= 62
    Mascara alfa = 1;                         // Paso 1: (0,...,0,1)

    while (alfa <= ultimo) {                  // Paso 4
        ++r.examinadas;
        int fi = filaResponsable(alfa, MB);   // Paso 2

        if (fi == -1) {                       // alfa es testor
            r.testores.push_back(alfa);
            int k = posicionUltimoUno(alfa, n);
            bool tipico;
            if (r.tipicos.size() < MB.size()) {
                tipico = tipicoPorAlmacenados(alfa, r.tipicos);
            } else {
                tipico = esTipicoDirecto(alfa, MB);
                if (tipico) r.tipicos.push_back(alfa);
            }
            Mascara sig = alfa + (1ULL << (n - k));          // proposicion 1.4
            if (guardarTraza)
                r.traza.push_back({alfa, true, -1, k, sig, tipico, r.tipicos});
            alfa = sig;
        } else {                              // alfa NO es testor
            int k = posicionUltimoUno(MB[fi], n);
            int p = n - k;
            Mascara sig = ((alfa >> (p + 1)) << (p + 1)) | (1ULL << p);  // prop. 1.5
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
void imprimirMatriz(const string& nombre, const vector<Mascara>& M, int n) {
    cout << "\nMatriz " << nombre << " (" << M.size() << " filas x " << n << " columnas):\n";
    cout << "      ";
    for (int j = 1; j <= n; ++j) cout << "x" << j << " ";
    cout << "\n";
    for (size_t i = 0; i < M.size(); ++i) {
        cout << left << setw(6) << ("F" + to_string(i + 1)) << right;
        for (int j = 0; j < n; ++j) cout << ((M[i] >> (n - 1 - j)) & 1ULL) << "  ";
        cout << "  " << mascaraAConjunto(M[i], n) << "\n";
    }
}

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
    cout << right;
}

// ============================================================================
// ENTRADA Y VALIDACION (misma logica que los Ejercicios 2 y 3)
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

bool leerDesdeArchivo(const string& queMatriz, vector<vector<int>>& M) {
    cout << "Ruta del archivo con la matriz " << queMatriz
         << " (Enter para matriz_basica.txt): ";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string ruta;
    getline(cin, ruta);
    if (ruta.empty()) ruta = "matriz_basica.txt";
    ifstream entrada(ruta);
    if (!entrada || !leerMatriz(entrada, M)) {
        cerr << "No se pudo leer una matriz booleana en " << ruta << ".\n";
        return false;
    }
    return true;
}

// ============================================================================
// PROGRAMA PRINCIPAL
// ============================================================================
int main() {
    cout << "====================================================================\n";
    cout << "   EJERCICIO 4.2: TESTORES TIPICOS DE LA MATRIZ B MEDIANTE BT\n";
    cout << "====================================================================\n\n";
    cout << "1. Construir B a partir de la Matriz Basica A del Ejercicio 2.1\n";
    cout << "2. Construir B a partir de una matriz basica A leida desde archivo\n";
    cout << "3. Leer la matriz B directamente desde un archivo\n";
    cout << "Opcion: ";
    int opcion;
    if (!(cin >> opcion) || opcion < 1 || opcion > 3) {
        cerr << "Opcion invalida. Elija 1, 2 o 3.\n";
        return 1;
    }

    // Matriz B obtenida a mano en el Ejercicio 4.1 (para contrastar con la opcion 1)
    const vector<vector<int>> B41 = {
        {0, 0, 1, 0, 0, 0, 1},
        {1, 1, 1, 0, 0, 0, 0},
        {1, 0, 1, 1, 0, 0, 0},
        {1, 1, 0, 0, 0, 1, 0},
        {1, 0, 0, 0, 0, 1, 1},
        {0, 1, 1, 0, 0, 1, 0},
        {1, 1, 0, 1, 1, 0, 0},
        {1, 0, 0, 1, 1, 0, 1}
    };

    vector<vector<int>> A, Bfilas;
    bool hayA = (opcion != 3);

    if (opcion == 1) {
        A = {
            {1, 0, 0, 0, 0, 1, 1},
            {1, 0, 1, 0, 0, 0, 0},
            {0, 0, 1, 0, 1, 1, 0},
            {0, 1, 0, 1, 0, 0, 1},
            {1, 1, 0, 0, 0, 0, 1},
            {0, 0, 1, 1, 0, 1, 0},
            {0, 1, 1, 0, 0, 0, 1}
        };
    } else if (opcion == 2) {
        if (!leerDesdeArchivo("A", A)) return 1;
    } else {
        if (!leerDesdeArchivo("B", Bfilas)) return 1;
    }

    vector<Mascara> MA, MB;
    int n = 0;

    // ---------------- Paso 1 y 2: A -> testores tipicos de A -> B -----------
    if (hayA) {
        if (!esMatrizBasica(A)) {
            cerr << "A debe ser una matriz basica no vacia, sin filas nulas, "
                 << "duplicadas ni superfilas.\n";
            return 1;
        }
        n = static_cast<int>(A[0].size());
        if (n > 62) {
            cerr << "Esta implementacion admite hasta 62 columnas.\n";
            return 1;
        }
        for (const auto& f : A) MA.push_back(filaAMascara(f));

        cout << "\n=== PASO 1: MATRIZ BASICA A ===\n";
        imprimirMatriz("A", MA, n);

        ResultadoBT rA = ejecutarBT(MA, n, false);
        cout << "\n=== PASO 2: CONSTRUCCION DE B (EJERCICIO 4.1) ===\n";
        cout << "Testores tipicos de A calculados con BT: " << rA.tipicos.size() << "\n";
        cout << "Cada testor tipico se escribe en notacion binaria como una fila de B.\n";
        MB = rA.tipicos;
        for (Mascara t : MB) Bfilas.push_back(mascaraAFila(t, n));
        imprimirMatriz("B", MB, n);

        if (opcion == 1) {
            vector<Mascara> m41;
            for (const auto& f : B41) m41.push_back(filaAMascara(f));
            sort(m41.begin(), m41.end());
            cout << "\n-> Comparacion con la matriz B del Ejercicio 4.1: "
                 << (m41 == MB ? "coinciden (mismas filas, posiblemente en otro orden).\n"
                               : "NO coinciden. Revisar el Ejercicio 4.1.\n");
        }
    } else {
        n = static_cast<int>(Bfilas.empty() ? 0 : Bfilas[0].size());
        if (n > 62) {
            cerr << "Esta implementacion admite hasta 62 columnas.\n";
            return 1;
        }
        for (const auto& f : Bfilas) MB.push_back(filaAMascara(f));
    }

    // ---------------- Paso 3: B debe ser basica ------------------------------
    cout << "\n=== PASO 3: VERIFICACION DE QUE B ES MATRIZ BASICA ===\n";
    if (!esMatrizBasica(Bfilas)) {
        cerr << "B no es una matriz basica (tiene filas nulas, duplicadas o superfilas).\n";
        return 1;
    }
    cout << "B no tiene filas nulas, duplicadas ni superfilas: B es una matriz basica.\n";

    // ---------------- Paso 4: BT sobre B -------------------------------------
    using Reloj = chrono::steady_clock;
    const int repeticiones = 1000;   // BT es muy rapido: se promedian corridas
    auto inicio = Reloj::now();
    ResultadoBT medido;
    for (int i = 0; i < repeticiones; ++i) medido = ejecutarBT(MB, n, false);
    double promedioMs =
        chrono::duration<double, milli>(Reloj::now() - inicio).count() / repeticiones;

    ResultadoBT r = ejecutarBT(MB, n, true);   // corrida con traza, fuera del cronometro

    cout << "\n=== PASO 4: ALGORITMO BT SOBRE LA MATRIZ B (EJERCICIO 4.2) ===\n";
    imprimirMatriz("B", MB, n);

    cout << "\n--- RECORRIDO DEL ARBOL BINARIO (solo listas examinadas) ---\n\n";
    const string nombreCsv = "bt_matrizB_resultados.csv";
    ofstream csv(nombreCsv);
    imprimirTraza(r, n, csv);

    cout << "\nListas examinadas: " << r.examinadas << " de " << ((1ULL << n) - 1)
         << " posibles (" << fixed << setprecision(2)
         << 100.0 * r.examinadas / ((1ULL << n) - 1) << "%)\n";

    cout << "\nTestores encontrados por BT (" << r.testores.size() << "):\n";
    for (Mascara t : r.testores)
        cout << "   " << mascaraACadena(t, n) << "  " << mascaraAConjunto(t, n) << "\n";

    cout << "\nTestores TIPICOS de B (" << r.tipicos.size() << "):\n";
    for (Mascara t : r.tipicos)
        cout << "   " << mascaraACadena(t, n) << "  " << mascaraAConjunto(t, n) << "\n";

    cout << "\nTiempo total de ejecucion de BT sobre B (promedio de " << repeticiones
         << " corridas): " << fixed << setprecision(6) << promedioMs << " ms\n";

    // ---------------- Paso 5: comparacion con A (apoyo al 4.3) ---------------
    if (hayA) {
        cout << "\n=== PASO 5: COMPARACION ENTRE TT(B) Y LAS FILAS DE A ===\n";
        for (Mascara t : r.tipicos) {
            auto it = find(MA.begin(), MA.end(), t);
            cout << "   " << left << setw(n + 2) << mascaraACadena(t, n) << setw(22)
                 << mascaraAConjunto(t, n) << right;
            if (it != MA.end()) cout << "= fila F" << (it - MA.begin() + 1) << " de A\n";
            else cout << "no es fila de A\n";
        }
        vector<Mascara> filasA = MA;
        sort(filasA.begin(), filasA.end());
        cout << "\n-> Los testores tipicos de B "
             << (filasA == r.tipicos ? "son exactamente las filas de A: TT(B) = A.\n"
                                     : "NO coinciden con las filas de A.\n");
    }

    csv.close();
    cout << "\nTraza guardada en " << nombreCsv << ".\n";
    return 0;
}
