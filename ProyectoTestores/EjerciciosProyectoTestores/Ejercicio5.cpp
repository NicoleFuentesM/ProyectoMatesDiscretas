//
//  Ejercicio5.cpp
//  Operadores phi, theta y gamma, y experimento de tiempos YYC vs BT.
//
//  5.1  Implementa los operadores del articulo "Generating synthetic test
//       matrices as a benchmark for the computational behavior of typical
//       testor-finding algorithms" y genera phi^N, theta^N y gamma^N a partir
//       de las matrices A (Ej. 2.1) y B (Ej. 4.1).
//  5.2  Construye una tabla para A y otra para B con las columnas:
//       Matriz | Filas | Columnas | Densidad | TT |
//       YYC (orden original) | YYC (orden ascendente) |
//       BT (orden original)  | BT (orden ascendente)
//
//  Los algoritmos son los mismos de Ejercicio2.cpp (YYC) y Ejercicio3.cpp (BT).
//  De YYC se copiaron tal cual FindCompatibleSet (Algoritmo 2) y
//  actualizarTestores (Algoritmo 1). BT conserva la misma logica; solo se le
//  quito la traza y se le agrego un corte por tiempo limite.
//
//  Compilar:  g++ -std=c++11 -O2 -o ej5 Ejercicio5.cpp
//

#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace std;

using Matriz = vector<vector<int>>;
using Mascara = uint64_t;
using Reloj = chrono::steady_clock;

// ============================================================================
// PARAMETROS DEL EXPERIMENTO (se pueden cambiar aqui)
// ============================================================================

// Valor maximo de N para cada operador, en el orden {phi, theta, gamma}.
// Con estos valores la tabla de cada matriz tarda unos minutos, casi todo en
// BT sobre las matrices mas grandes. Para una corrida rapida usar {5, 3, 4}.
const int N_MAX_A[3] = {6, 4, 5};
const int N_MAX_B[3] = {6, 4, 5};

// Si una corrida de un algoritmo supera este tiempo se corta y en la tabla
// aparece "> limite".
const double LIMITE_SEGUNDOS = 300.0;

// Las corridas muy rapidas se repiten hasta acumular este tiempo (o hasta el
// maximo de repeticiones) y se reporta el promedio. Se aplica igual a YYC y BT.
const double TIEMPO_MINIMO_MS = 200.0;
const int MAX_REPETICIONES = 1000;

// Limites de tamano de esta implementacion.
const int MAX_COLUMNAS = 62;                 // BT guarda cada lista en 64 bits
const unsigned long long MAX_FILAS = 300000ULL;
const unsigned long long MAX_TT = 2000000ULL;

// ============================================================================
// MATRICES BASE
// ============================================================================

// Matriz basica A del Ejercicio 2.1 (7 x 7, densidad 0.4082)
const Matriz MATRIZ_A = {
    {1, 0, 0, 0, 0, 1, 1},
    {1, 0, 1, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 1, 0},
    {0, 1, 0, 1, 0, 0, 1},
    {1, 1, 0, 0, 0, 0, 1},
    {0, 0, 1, 1, 0, 1, 0},
    {0, 1, 1, 0, 0, 0, 1}
};

// Matriz B del Ejercicio 4.1 (8 x 7): los testores tipicos de A
const Matriz MATRIZ_B = {
    {0, 0, 1, 0, 0, 0, 1},
    {1, 1, 1, 0, 0, 0, 0},
    {1, 0, 1, 1, 0, 0, 0},
    {1, 1, 0, 0, 0, 1, 0},
    {1, 0, 0, 0, 0, 1, 1},
    {0, 1, 1, 0, 0, 1, 0},
    {1, 1, 0, 1, 1, 0, 0},
    {1, 0, 0, 1, 1, 0, 1}
};

// ============================================================================
// 5.1  OPERADORES phi, theta Y gamma
// ============================================================================

// phi(A,B) = [A B]   (fusion simple)
// Requiere que A y B tengan el mismo numero de filas.
// Resultado: m filas, n + n' columnas.
Matriz opPhi(const Matriz& A, const Matriz& B) {
    Matriz R;
    for (size_t i = 0; i < A.size(); ++i) {
        vector<int> fila = A[i];
        fila.insert(fila.end(), B[i].begin(), B[i].end());
        R.push_back(fila);
    }
    return R;
}

// theta(A,B)   (fusion combinatoria)
// Cada fila de A se concatena con cada una de las filas de B.
// Resultado: m * m' filas, n + n' columnas.
Matriz opTheta(const Matriz& A, const Matriz& B) {
    Matriz R;
    for (const auto& filaA : A) {
        for (const auto& filaB : B) {
            vector<int> fila = filaA;
            fila.insert(fila.end(), filaB.begin(), filaB.end());
            R.push_back(fila);
        }
    }
    return R;
}

// gamma(A,B) = [A 0; 0 B]   (bloques en diagonal)
// Resultado: m + m' filas, n + n' columnas.
Matriz opGamma(const Matriz& A, const Matriz& B) {
    const size_t nA = A[0].size();
    const size_t nB = B[0].size();
    Matriz R;
    for (const auto& filaA : A) {
        vector<int> fila = filaA;
        fila.insert(fila.end(), nB, 0);          // ceros a la derecha de A
        R.push_back(fila);
    }
    for (const auto& filaB : B) {
        vector<int> fila(nA, 0);                 // ceros a la izquierda de B
        fila.insert(fila.end(), filaB.begin(), filaB.end());
        R.push_back(fila);
    }
    return R;
}

using Operador = Matriz (*)(const Matriz&, const Matriz&);
const Operador OPERADORES[3] = {opPhi, opTheta, opGamma};
const char* const NOMBRE_OP[3] = {"phi", "theta", "gamma"};
const char* const LATEX_OP[3] = {"\\varphi", "\\theta", "\\gamma"};

// O^N(M): el operador aplicado N veces sobre la misma matriz.
// O^1(M) = M,  O^N(M) = O(O^(N-1)(M), M).
Matriz potencia(Operador op, const Matriz& M, int N) {
    Matriz R = M;
    for (int i = 1; i < N; ++i) R = op(R, M);
    return R;
}

// ============================================================================
// UTILIDADES SOBRE MATRICES
// ============================================================================

// Densidad: unos / (filas * columnas)   (misma formula del Ejercicio 1)
double calcularDensidad(const Matriz& M) {
    size_t unos = 0;
    for (const auto& fila : M)
        for (int v : fila)
            if (v == 1) ++unos;
    return static_cast<double>(unos) /
           (static_cast<double>(M.size()) * static_cast<double>(M[0].size()));
}

// Ordena las filas de forma ascendente por su cantidad de unos. El orden
// relativo de las filas empatadas se conserva (stable_sort).
Matriz ordenarAscendente(const Matriz& M) {
    Matriz R = M;
    stable_sort(R.begin(), R.end(),
                [](const vector<int>& a, const vector<int>& b) {
                    return count(a.begin(), a.end(), 1) < count(b.begin(), b.end(), 1);
                });
    return R;
}

void imprimirMatriz(ostream& salida, const Matriz& M) {
    for (const auto& fila : M) {
        for (size_t j = 0; j < fila.size(); ++j) {
            if (j != 0) salida << ' ';
            salida << fila[j];
        }
        salida << '\n';
    }
}

// ============================================================================
// ALGORITMO YYC
// FindCompatibleSet y actualizarTestores estan copiadas tal cual de
// Ejercicio2.cpp (version con el Algoritmo 2 del articulo).
// ============================================================================

// ALGORITMO 2. FindCompatibleSet(tau, xp)
// tau es un testor típico hasta la fila anterior y no cubre la fila actual.
// xp es una columna con 1 en la fila actual, así que no pertenece a tau.
// El candidato tau ∪ {xp} ya cubre todas las filas vistas. Aquí comprobamos
// si cada columna del candidato es indispensable, es decir, si es típico.
bool FindCompatibleSet(const set<int>& tau, int xp,
                       const vector<vector<int>>& BM, int filaActual) {
    // Paso 1. RefSM es la submatriz de las filas de 0 a filaActual, inclusive,
    // y de las columnas tau ∪ {xp}. La recorremos en BM usando sus índices.
    vector<int> columnas(tau.begin(), tau.end());
    columnas.push_back(xp);

    // En una pasada guardamos dos datos del filtro del artículo: cuántas
    // filas tienen un solo 1 y qué columnas reciben ese 1 en alguna de ellas.
    size_t filasConUnUno = 0;
    vector<bool> columnaConTestigo(columnas.size(), false);
    for (int fila = 0; fila <= filaActual; ++fila) {
        int sumaFila = 0;
        size_t posicionUno = 0;
        for (size_t k = 0; k < columnas.size(); ++k) {
            if (BM[fila][columnas[k]] == 1) {
                ++sumaFila;
                if (sumaFila == 2) break; // Con dos unos ya se descarta la fila.
                // k es la posición local. columnas[k] es su índice en BM.
                posicionUno = k;
            }
        }

        // Paso 2. Solo se consideran las filas cuya suma en RefSM es 1.
        // Esa fila es un testigo de su única columna con 1. Si quitamos esa
        // columna, la fila queda en ceros dentro del candidato.
        if (sumaFila == 1) {
            ++filasConUnUno;
            columnaConTestigo[posicionUno] = true;
        }
    }

    // Paso 3. Condición 1: hay al menos tantas filas filtradas como columnas.
    const bool condicion1 = filasConUnUno >= columnas.size();
    // Paso 4. Condición 2: cada columna tiene algún 1 entre las filas filtradas.
    // Varias filas pueden ser testigos de la misma columna. Por eso, tener
    // suficientes filas no garantiza que todas las columnas tengan testigo.
    // Ejemplo de A: hasta r_6, el candidato {x1, x2, x4, x5} tiene cuatro
    // filas testigo, pero dos corresponden a x1 y ninguna a x2. Se descarta
    // en esa iteración. Puede aceptarse en r_7, que aporta el testigo de x2.
    const bool condicion2 = all_of(columnaConTestigo.begin(), columnaConTestigo.end(),
                                   [](bool tieneTestigo) { return tieneTestigo; });
    // Paso 5. Si ambas condiciones se cumplen, podemos elegir una fila testigo
    // por columna y ordenarlas para formar la matriz identidad del conjunto compatible.
    return condicion1 && condicion2;
}

// ALGORITMO 1. Actualización al incorporar la fila r_i.
set<set<int>> actualizarTestores(const vector<vector<int>>& BM, int fila,
                               const set<set<int>>& testoresPrevios) {
    // testoresPrevios es ψ* hasta la fila anterior. Empezamos ψAux vacío.
    // set<set<int>> guarda conjuntos de columnas y evita resultados repetidos.
    set<set<int>> testoresActuales;
    // Solo una columna con 1 en r_i puede cubrirla cuando tau no la cubre.
    // Reunimos esas columnas una vez para usarlas con todos los testores previos.
    vector<int> columnasConUno;
    for (int col = 0; col < static_cast<int>(BM[fila].size()); ++col) {
        if (BM[fila][col] == 1) columnasConUno.push_back(col);
    }

    for (const auto& testor : testoresPrevios) {
        // Caso 1. Basta un 1 de tau en r_i para conservarlo. Sus columnas
        // siguen siendo indispensables porque conservan sus testigos anteriores.
        const bool cubreFila = any_of(testor.begin(), testor.end(),
                                     [&](int col) { return BM[fila][col] == 1; });
        if (cubreFila) {
            testoresActuales.insert(testor);
            continue;
        }

        // Caso 2. tau deja r_i en ceros. Probamos agregar un xp a la vez.
        // La unión conserva la cobertura anterior y xp cubre r_i.
        for (int col : columnasConUno) {
            // Solo agregamos el candidato a ψAux si pasa el Algoritmo 2.
            if (FindCompatibleSet(testor, col, BM, fila)) {
                set<int> candidato = testor;
                candidato.insert(col);
                testoresActuales.insert(std::move(candidato));
            }
        }
    }
    // Entregamos ψAux para que pase a ser el nuevo ψ*.
    return testoresActuales;
}

struct ResultadoYYC {
    set<set<int>> tipicos;
    size_t maximoIntermedio = 0;   // mayor cantidad de testores acumulados
    bool completo = true;          // false si se corto por tiempo
};

// Mismo recorrido fila por fila de ejecutarAlgoritmoYYC (Ej. 2), sin imprimir.
ResultadoYYC ejecutarYYC(const Matriz& BM, Reloj::time_point limite) {
    ResultadoYYC r;
    for (int col = 0; col < static_cast<int>(BM[0].size()); ++col) {
        if (BM[0][col] == 1) r.tipicos.insert({col});
    }
    r.maximoIntermedio = r.tipicos.size();

    for (int fila = 1; fila < static_cast<int>(BM.size()); ++fila) {
        if (Reloj::now() > limite) {
            r.completo = false;
            break;
        }
        r.tipicos = actualizarTestores(BM, fila, r.tipicos);
        r.maximoIntermedio = max(r.maximoIntermedio, r.tipicos.size());
    }
    return r;
}

// ============================================================================
// ALGORITMO BT  (misma logica de Ejercicio3.cpp, sin la traza)
// Cada lista se guarda en un entero: x1 es el bit mas significativo.
// ============================================================================

Mascara filaAMascara(const vector<int>& fila) {
    Mascara m = 0;
    for (int v : fila) m = (m << 1) | static_cast<Mascara>(v);
    return m;
}

int posicionUltimoUno(Mascara m, int n) {
    int p = __builtin_ctzll(m);
    return n - p;
}

bool esTestor(Mascara alfa, const vector<Mascara>& MB) {
    for (Mascara a : MB)
        if ((alfa & a) == 0) return false;
    return true;
}

int filaResponsable(Mascara alfa, const vector<Mascara>& MB) {
    int mejor = -1;
    int mejorK = numeric_limits<int>::max();
    for (size_t i = 0; i < MB.size(); ++i) {
        if ((alfa & MB[i]) == 0) {
            int p = __builtin_ctzll(MB[i]);
            int k = -p;
            if (mejor == -1 || k < mejorK) {
                mejor = static_cast<int>(i);
                mejorK = k;
            }
        }
    }
    return mejor;
}

bool esTipicoDirecto(Mascara alfa, const vector<Mascara>& MB) {
    Mascara resto = alfa;
    while (resto) {
        Mascara bit = resto & (~resto + 1);
        if (esTestor(alfa ^ bit, MB)) return false;
        resto ^= bit;
    }
    return true;
}

bool tipicoPorAlmacenados(Mascara alfa, vector<Mascara>& tipicos) {
    for (Mascara t : tipicos)
        if ((t & alfa) == t) return false;
    tipicos.erase(remove_if(tipicos.begin(), tipicos.end(),
                            [&](Mascara t) { return (alfa & t) == alfa; }),
                  tipicos.end());
    tipicos.push_back(alfa);
    return true;
}

struct ResultadoBT {
    vector<Mascara> tipicos;
    long long examinadas = 0;      // listas examinadas por BT
    bool completo = true;          // false si se corto por tiempo
};

ResultadoBT ejecutarBT(const vector<Mascara>& MB, int n, Reloj::time_point limite) {
    ResultadoBT r;
    const Mascara ultimo = (1ULL << n) - 1;
    Mascara alfa = 1;

    while (alfa <= ultimo) {
        // Corte por tiempo: se revisa el reloj cada 1024 listas.
        if ((r.examinadas & 1023) == 0 && Reloj::now() > limite) {
            r.completo = false;
            break;
        }
        ++r.examinadas;
        int fi = filaResponsable(alfa, MB);

        if (fi == -1) {                    // alfa es testor
            int k = posicionUltimoUno(alfa, n);
            if (r.tipicos.size() < MB.size()) {
                tipicoPorAlmacenados(alfa, r.tipicos);
            } else {
                if (esTipicoDirecto(alfa, MB)) r.tipicos.push_back(alfa);
            }
            alfa = alfa + (1ULL << (n - k));
        } else {                           // alfa no es testor
            int k = posicionUltimoUno(MB[fi], n);
            int p = n - k;
            alfa = ((alfa >> (p + 1)) << (p + 1)) | (1ULL << p);
        }
    }
    sort(r.tipicos.begin(), r.tipicos.end());
    return r;
}

// ============================================================================
// MEDICION DE TIEMPOS
// ============================================================================

struct Medicion {
    double ms = 0.0;               // tiempo promedio por corrida
    bool completo = true;
    int repeticiones = 0;
    vector<Mascara> tipicos;       // testores tipicos encontrados (ordenados)
    unsigned long long apoyo = 0;  // YYC: maximo de testores intermedios
                                   // BT : listas examinadas
};

Reloj::time_point calcularLimite() {
    return Reloj::now() + chrono::duration_cast<Reloj::duration>(
                              chrono::duration<double>(LIMITE_SEGUNDOS));
}

// Pasa los testores de YYC (conjuntos de columnas) al formato de mascaras de
// BT para poder comparar los resultados de los dos algoritmos.
vector<Mascara> conjuntosAMascaras(const set<set<int>>& testores, int n) {
    vector<Mascara> r;
    for (const auto& t : testores) {
        Mascara m = 0;
        for (int col : t) m |= 1ULL << (n - 1 - col);
        r.push_back(m);
    }
    sort(r.begin(), r.end());
    return r;
}

Medicion medirYYC(const Matriz& M) {
    Medicion med;
    double totalMs = 0.0;
    ResultadoYYC r;
    do {
        auto inicio = Reloj::now();
        r = ejecutarYYC(M, calcularLimite());
        totalMs += chrono::duration<double, milli>(Reloj::now() - inicio).count();
        ++med.repeticiones;
    } while (r.completo && totalMs < TIEMPO_MINIMO_MS &&
             med.repeticiones < MAX_REPETICIONES);

    med.completo = r.completo;
    med.ms = totalMs / med.repeticiones;
    med.apoyo = r.maximoIntermedio;
    if (r.completo) med.tipicos = conjuntosAMascaras(r.tipicos, static_cast<int>(M[0].size()));
    return med;
}

Medicion medirBT(const Matriz& M) {
    const int n = static_cast<int>(M[0].size());
    vector<Mascara> MB;
    for (const auto& fila : M) MB.push_back(filaAMascara(fila));

    Medicion med;
    double totalMs = 0.0;
    ResultadoBT r;
    do {
        auto inicio = Reloj::now();
        r = ejecutarBT(MB, n, calcularLimite());
        totalMs += chrono::duration<double, milli>(Reloj::now() - inicio).count();
        ++med.repeticiones;
    } while (r.completo && totalMs < TIEMPO_MINIMO_MS &&
             med.repeticiones < MAX_REPETICIONES);

    med.completo = r.completo;
    med.ms = totalMs / med.repeticiones;
    med.apoyo = static_cast<unsigned long long>(r.examinadas);
    if (r.completo) med.tipicos = r.tipicos;
    return med;
}

// ============================================================================
// NUMERO DE TESTORES TIPICOS CONOCIDO DE ANTEMANO (corolarios del articulo)
//   |TT(phi^N(M))|   = suma de N^|T| sobre los testores tipicos T de M
//   |TT(theta^N(M))| = N * |TT(M)|
//   |TT(gamma^N(M))| = |TT(M)|^N
// ============================================================================

unsigned long long potenciaEntera(unsigned long long base, int exp) {
    unsigned long long r = 1;
    for (int i = 0; i < exp; ++i) r *= base;
    return r;
}

// tamanos: cardinalidad de cada testor tipico de la matriz base
unsigned long long ttTeorico(int op, int N, const vector<size_t>& tamanos) {
    const unsigned long long t = tamanos.size();
    if (op == 0) {
        unsigned long long suma = 0;
        for (size_t tam : tamanos)
            suma += potenciaEntera(static_cast<unsigned long long>(N), static_cast<int>(tam));
        return suma;
    }
    if (op == 1) return static_cast<unsigned long long>(N) * t;
    return potenciaEntera(t, N);
}

vector<size_t> tamanosTestoresTipicos(const Matriz& base) {
    ResultadoYYC r = ejecutarYYC(base, Reloj::time_point::max());
    vector<size_t> tamanos;
    for (const auto& t : r.tipicos) tamanos.push_back(t.size());
    return tamanos;
}

// Dimensiones de O^N(M) sin construir la matriz.
void dimensiones(int op, int N, const Matriz& base,
                 unsigned long long& filas, unsigned long long& columnas) {
    const unsigned long long m = base.size();
    columnas = static_cast<unsigned long long>(base[0].size()) * N;
    if (op == 0) filas = m;
    else if (op == 1) filas = potenciaEntera(m, N);
    else filas = m * N;
}

// ============================================================================
// 5.2  CONSTRUCCION DE LAS TABLAS
// ============================================================================

struct FilaTabla {
    string nombre;        // p. ej. "theta^2(A)"
    string nombreLatex;   // p. ej. "$\theta^{2}(A)$"
    size_t filas = 0;
    size_t columnas = 0;
    double densidad = 0.0;
    unsigned long long tt = 0;
    Medicion yycOriginal, yycAscendente, btOriginal, btAscendente;
    bool coincide = true; // los algoritmos devolvieron el mismo conjunto
};

string formatoTiempo(const Medicion& m, bool latex) {
    ostringstream s;
    if (!m.completo) {
        s << (latex ? "$>$" : ">") << fixed << setprecision(0) << LIMITE_SEGUNDOS << " s";
    } else {
        s << fixed << setprecision(4) << m.ms;
    }
    return s.str();
}

// Genera O^N(base), mide los cuatro tiempos y verifica los resultados.
FilaTabla procesarMatriz(int op, int N, const Matriz& base, char letra,
                         const vector<size_t>& tamanos, Matriz* matrizGenerada) {
    FilaTabla f;
    f.nombre = string(NOMBRE_OP[op]) + "^" + to_string(N) + "(" + letra + ")";
    f.nombreLatex = string("$") + LATEX_OP[op] + "^{" + to_string(N) + "}(" + letra + ")$";

    const Matriz original = potencia(OPERADORES[op], base, N);
    const Matriz ascendente = ordenarAscendente(original);
    if (matrizGenerada != nullptr) *matrizGenerada = original;

    f.filas = original.size();
    f.columnas = original[0].size();
    f.densidad = calcularDensidad(original);
    f.tt = ttTeorico(op, N, tamanos);

    f.yycOriginal = medirYYC(original);
    f.yycAscendente = medirYYC(ascendente);
    f.btOriginal = medirBT(original);
    f.btAscendente = medirBT(ascendente);

    // Verificacion: toda corrida que termino debe dar el mismo conjunto de
    // testores tipicos, y su cantidad debe ser la que predice el articulo.
    const Medicion* corridas[4] = {&f.yycOriginal, &f.yycAscendente,
                                   &f.btOriginal, &f.btAscendente};
    const Medicion* referencia = nullptr;
    for (const Medicion* c : corridas) {
        if (!c->completo) continue;
        if (c->tipicos.size() != f.tt) f.coincide = false;
        if (referencia == nullptr) referencia = c;
        else if (c->tipicos != referencia->tipicos) f.coincide = false;
    }
    return f;
}

void imprimirEncabezado() {
    cout << left << setw(13) << "Matriz" << right << setw(7) << "Filas" << setw(6)
         << "Cols" << setw(10) << "Densidad" << setw(8) << "TT" << setw(14)
         << "YYC orig" << setw(14) << "YYC asc" << setw(14) << "BT orig"
         << setw(14) << "BT asc" << "\n";
    cout << string(100, '-') << "\n";
}

void imprimirFila(const FilaTabla& f) {
    cout << left << setw(13) << f.nombre << right << setw(7) << f.filas << setw(6)
         << f.columnas << setw(10) << fixed << setprecision(4) << f.densidad
         << setw(8) << f.tt
         << setw(14) << formatoTiempo(f.yycOriginal, false)
         << setw(14) << formatoTiempo(f.yycAscendente, false)
         << setw(14) << formatoTiempo(f.btOriginal, false)
         << setw(14) << formatoTiempo(f.btAscendente, false);
    if (!f.coincide) cout << "  <-- RESULTADOS DISTINTOS";
    cout << endl;
}

void imprimirApoyo(const vector<FilaTabla>& tabla) {
    cout << "\nDatos de apoyo para el analisis (5.3):\n";
    cout << left << setw(13) << "Matriz" << right << setw(18) << "YYC max interm."
         << setw(18) << "YYC max interm." << setw(16) << "BT listas"
         << setw(16) << "BT listas" << "\n";
    cout << left << setw(13) << "" << right << setw(18) << "(original)"
         << setw(18) << "(ascendente)" << setw(16) << "(original)"
         << setw(16) << "(ascendente)" << "\n";
    cout << string(81, '-') << "\n";
    for (const FilaTabla& f : tabla) {
        cout << left << setw(13) << f.nombre << right
             << setw(18) << f.yycOriginal.apoyo << setw(18) << f.yycAscendente.apoyo
             << setw(16) << f.btOriginal.apoyo << setw(16) << f.btAscendente.apoyo << "\n";
    }
}

void guardarCsv(const vector<FilaTabla>& tabla, const string& ruta) {
    ofstream csv(ruta);
    if (!csv) {
        cerr << "No se pudo abrir " << ruta << " para escritura.\n";
        return;
    }
    csv << "matriz,filas,columnas,densidad,tt,yyc_original_ms,yyc_ascendente_ms,"
        << "bt_original_ms,bt_ascendente_ms,yyc_max_intermedios_original,"
        << "yyc_max_intermedios_ascendente,bt_listas_original,bt_listas_ascendente,"
        << "resultados_coinciden\n";
    for (const FilaTabla& f : tabla) {
        csv << f.nombre << ',' << f.filas << ',' << f.columnas << ','
            << fixed << setprecision(4) << f.densidad << ',' << f.tt << ','
            << formatoTiempo(f.yycOriginal, false) << ','
            << formatoTiempo(f.yycAscendente, false) << ','
            << formatoTiempo(f.btOriginal, false) << ','
            << formatoTiempo(f.btAscendente, false) << ','
            << f.yycOriginal.apoyo << ',' << f.yycAscendente.apoyo << ','
            << f.btOriginal.apoyo << ',' << f.btAscendente.apoyo << ','
            << (f.coincide ? "si" : "no") << '\n';
    }
    cout << "Tabla guardada en " << ruta << ".\n";
}

// Tabla lista para incluir en el reporte con \input{...}
void guardarLatex(const vector<FilaTabla>& tabla, const string& ruta, char letra) {
    ofstream tex(ruta);
    if (!tex) {
        cerr << "No se pudo abrir " << ruta << " para escritura.\n";
        return;
    }
    tex << "\\begin{table}[htbp]\n\\centering\n\\small\n";
    tex << "\\caption{Matrices generadas a partir de $" << letra
        << "$ y tiempos de ejecuci\\'on (ms) de YYC y BT.}\n";
    tex << "\\label{tab:ej5" << letra << "}\n";
    tex << "\\begin{tabular}{lrrrrrrrr}\n\\hline\n";
    tex << " & & & & & \\multicolumn{2}{c}{YYC} & \\multicolumn{2}{c}{BT} \\\\\n";
    tex << "Matriz & Filas & Columnas & Densidad & TT & Original & Ascendente "
        << "& Original & Ascendente \\\\\n\\hline\n";
    string operadorAnterior;
    for (const FilaTabla& f : tabla) {
        const string operador = f.nombre.substr(0, f.nombre.find('^'));
        if (!operadorAnterior.empty() && operador != operadorAnterior) tex << "\\hline\n";
        operadorAnterior = operador;
        tex << f.nombreLatex << " & " << f.filas << " & " << f.columnas << " & "
            << fixed << setprecision(4) << f.densidad << " & " << f.tt << " & "
            << formatoTiempo(f.yycOriginal, true) << " & "
            << formatoTiempo(f.yycAscendente, true) << " & "
            << formatoTiempo(f.btOriginal, true) << " & "
            << formatoTiempo(f.btAscendente, true) << " \\\\\n";
    }
    tex << "\\hline\n\\end{tabular}\n\\end{table}\n";
    cout << "Tabla LaTeX guardada en " << ruta << ".\n";
}

// Tabla completa de una matriz base: phi^N, theta^N y gamma^N.
void construirTabla(const Matriz& base, char letra, const int nMax[3]) {
    cout << "\n==================== TABLA PARA LA MATRIZ " << letra
         << " ====================\n";
    cout << "Tiempos en milisegundos. Limite por corrida: " << fixed
         << setprecision(0) << LIMITE_SEGUNDOS << " s.\n\n";

    const vector<size_t> tamanos = tamanosTestoresTipicos(base);
    vector<FilaTabla> tabla;
    imprimirEncabezado();
    for (int op = 0; op < 3; ++op) {
        for (int N = 1; N <= nMax[op]; ++N) {
            tabla.push_back(procesarMatriz(op, N, base, letra, tamanos, nullptr));
            imprimirFila(tabla.back());
        }
        cout << string(100, '-') << "\n";
    }

    const bool todoCoincide = all_of(tabla.begin(), tabla.end(),
                                     [](const FilaTabla& f) { return f.coincide; });
    cout << (todoCoincide
                 ? "Verificacion: YYC y BT devolvieron el mismo conjunto de testores "
                   "tipicos en ambos ordenes,\ny su cantidad coincide con el TT del articulo.\n"
                 : "ATENCION: hay matrices con resultados distintos (ver marcas).\n");

    imprimirApoyo(tabla);
    cout << "\n";
    guardarCsv(tabla, string("tabla_ej5_") + letra + ".csv");
    guardarLatex(tabla, string("tabla_ej5_") + letra + ".tex", letra);
}

// Una sola matriz, para demostrar la ejecucion y los tiempos.
void ejecutarUnaMatriz() {
    char letra;
    cout << "Matriz base (A o B): ";
    if (!(cin >> letra)) return;
    if (letra == 'a') letra = 'A';
    if (letra == 'b') letra = 'B';
    if (letra != 'A' && letra != 'B') {
        cout << "La matriz base debe ser A o B.\n";
        return;
    }
    const Matriz& base = (letra == 'A') ? MATRIZ_A : MATRIZ_B;

    int op;
    cout << "Operador (1 = phi, 2 = theta, 3 = gamma): ";
    if (!(cin >> op) || op < 1 || op > 3) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "El operador debe ser 1, 2 o 3.\n";
        return;
    }
    --op;

    int N;
    cout << "Valor de N: ";
    if (!(cin >> N) || N < 1 || N > 20) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "N debe ser un entero entre 1 y 20.\n";
        return;
    }

    const vector<size_t> tamanos = tamanosTestoresTipicos(base);
    unsigned long long filas, columnas;
    dimensiones(op, N, base, filas, columnas);
    if (columnas > static_cast<unsigned long long>(MAX_COLUMNAS) || filas > MAX_FILAS ||
        ttTeorico(op, N, tamanos) > MAX_TT) {
        cout << "La matriz " << NOMBRE_OP[op] << "^" << N << "(" << letra << ") tendria "
             << filas << " filas, " << columnas << " columnas y "
             << ttTeorico(op, N, tamanos) << " testores tipicos.\n"
             << "Supera los limites de esta implementacion (" << MAX_COLUMNAS
             << " columnas, " << MAX_FILAS << " filas, " << MAX_TT << " testores).\n";
        return;
    }

    cout << "\nGenerando y midiendo " << NOMBRE_OP[op] << "^" << N << "(" << letra
         << ") ...\n\n";
    Matriz generada;
    FilaTabla f = procesarMatriz(op, N, base, letra, tamanos, &generada);

    if (generada.size() <= 16 && generada[0].size() <= 28) {
        cout << "Matriz generada:\n";
        imprimirMatriz(cout, generada);
        cout << "\n";
    }
    imprimirEncabezado();
    imprimirFila(f);
    cout << "\nRepeticiones promediadas: YYC orig " << f.yycOriginal.repeticiones
         << ", YYC asc " << f.yycAscendente.repeticiones
         << ", BT orig " << f.btOriginal.repeticiones
         << ", BT asc " << f.btAscendente.repeticiones << "\n";
    cout << (f.coincide ? "Verificacion: los resultados coinciden entre si y con el TT esperado.\n"
                        : "ATENCION: los resultados no coinciden.\n");
    imprimirApoyo(vector<FilaTabla>(1, f));

    // Se guarda con el mismo formato del Ejercicio 1, para poder cargarla en
    // los programas de los Ejercicios 2 y 3 (opcion "leer desde archivo").
    const string ruta = string(NOMBRE_OP[op]) + to_string(N) + "_" + letra + ".txt";
    ofstream archivo(ruta);
    if (archivo) {
        archivo << generada.size() << ' ' << generada[0].size() << '\n';
        imprimirMatriz(archivo, generada);
        cout << "\nMatriz guardada en " << ruta << ".\n";
    }
}

int main() {
    cout << "====================================================================\n";
    cout << "   OPERADORES phi, theta, gamma Y TIEMPOS DE YYC Y BT (EJ. 5)\n";
    cout << "====================================================================\n";

    while (true) {
        cout << "\n1. Tabla completa para la matriz A\n";
        cout << "2. Tabla completa para la matriz B\n";
        cout << "3. Ambas tablas\n";
        cout << "4. Generar y medir una sola matriz\n";
        cout << "0. Salir\n";
        cout << "Opcion: ";
        int opcion;
        if (!(cin >> opcion)) {
            if (cin.eof()) break;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Opcion invalida.\n";
            continue;
        }
        if (opcion == 0) break;
        if (opcion == 1 || opcion == 3) construirTabla(MATRIZ_A, 'A', N_MAX_A);
        if (opcion == 2 || opcion == 3) construirTabla(MATRIZ_B, 'B', N_MAX_B);
        if (opcion == 4) ejecutarUnaMatriz();
        if (opcion < 0 || opcion > 4) cout << "Opcion invalida.\n";
    }
    return 0;
}
