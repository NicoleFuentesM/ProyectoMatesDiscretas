//
//  Ejercicio5.cpp
//  Operadores phi, theta y gamma, y experimento de tiempos YYC vs BT.
//
//  5.1  Implementa los operadores del articulo "Generating synthetic test
//       matrices as a benchmark for the computational behavior of typical
//       testor-finding algorithms" y genera las matrices de prueba a partir
//       de A (Ej. 2.1) y B (Ej. 4.1).
//  5.2  Tablas principales, con el procedimiento de las Tablas 5 y 6 del
//       articulo: phi^N(theta(A,B)) y gamma^N(theta(A,B)). Columnas:
//       N | Matriz | Filas | Columnas | TT |
//       YYC (orden original) | YYC (orden ascendente) |
//       BT (orden original)  | BT (orden ascendente)
//       Como experimento adicional se generan tambien phi^N, theta^N y
//       gamma^N de A y de B por separado.
//
//  Ademas de los tiempos se cuentan los "hits", la medida de eficiencia de
//  los articulos: en YYC, cada candidato que se comprueba con
//  FindCompatibleSet; en BT, cada lista examinada. La opcion 6 compara esos
//  hits con los publicados, usando las matrices de los propios articulos.
//
//  Los algoritmos son los mismos de Ejercicio2.cpp (YYC) y Ejercicio3.cpp (BT).
//  De YYC se copiaron FindCompatibleSet (Algoritmo 2) y actualizarTestores
//  (Algoritmo 1); la unica linea agregada es el contador de hits. BT conserva
//  la misma logica; solo se le quito la traza y se le agrego un corte por
//  tiempo limite.
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

// Tablas principales, sobre theta(A,B). En el articulo la Tabla 5 llega a
// N = 5 y la Tabla 6 a N = 4.
const int N_MAX_PHI_PRINCIPAL = 5;
const int N_MAX_GAMMA_PRINCIPAL = 4;

// Experimento adicional: valor maximo de N para cada operador, en el orden
// {phi, theta, gamma}, sobre A y sobre B por separado.
const int N_MAX_A[3] = {6, 4, 5};
const int N_MAX_B[3] = {6, 4, 5};

// Si una corrida de un algoritmo supera este tiempo se corta y en la tabla
// aparece "> limite". Dentro de una misma familia de matrices, un algoritmo
// que ya supero el limite no se vuelve a ejecutar para los N siguientes.
const double LIMITE_SEGUNDOS = 300.0;

// Las corridas muy rapidas se repiten hasta acumular este tiempo (o hasta el
// maximo de repeticiones) y se reporta el promedio. Se aplica igual a YYC y BT.
const double TIEMPO_MINIMO_MS = 200.0;
const int MAX_REPETICIONES = 1000000;

// Limites de tamano de esta implementacion.
const int MAX_COLUMNAS_BT = 62;              // BT guarda cada lista en 64 bits
const unsigned long long MAX_COLUMNAS = 200ULL;
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
// Requiere que A y B tengan el mismo numero de filas; si no lo tienen, el
// operador no esta definido y se devuelve una matriz vacia.
// Resultado: m filas, n + n' columnas.
Matriz opPhi(const Matriz& A, const Matriz& B) {
    if (A.size() != B.size()) {
        cerr << "phi(A,B) requiere matrices con el mismo numero de filas ("
             << A.size() << " y " << B.size() << ").\n";
        return Matriz();
    }
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

// Matriz de partida de una familia: A, B o theta(A,B).
struct Base {
    string nombre;         // para la consola:  "A", "B", "theta(A,B)"
    string nombreArchivo;  // sin comas ni parentesis, para CSV y archivos
    string nombreLatex;    // para el reporte
    Matriz M;
};

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
// FindCompatibleSet y actualizarTestores vienen de Ejercicio2.cpp (version con
// el Algoritmo 2 del articulo). Solo se agrego el contador de hits.
// ============================================================================

// Hits de la corrida de YYC en curso: candidatos tau U {xp} comprobados.
unsigned long long hitsYYC = 0;

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
            // (Hit) Cada candidato que se comprueba cuenta como un hit, igual
            // que en el Algoritmo 1 del articulo. Es la unica linea agregada
            // respecto de Ejercicio2.cpp.
            ++hitsYYC;
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
    unsigned long long hits = 0;   // candidatos comprobados con FindCompatibleSet
    bool completo = true;          // false si se corto por tiempo
};

// Mismo recorrido fila por fila de ejecutarAlgoritmoYYC (Ej. 2), sin imprimir.
ResultadoYYC ejecutarYYC(const Matriz& BM, Reloj::time_point limite) {
    ResultadoYYC r;
    hitsYYC = 0;
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
    r.hits = hitsYYC;
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
// MEDICION DE TIEMPOS Y DE HITS
// ============================================================================

enum Estado {
    COMPLETO,       // la corrida termino
    CORTADO,        // se interrumpio al superar LIMITE_SEGUNDOS
    NO_EJECUTADO    // no se ejecuto (ver el pie de la tabla)
};

struct Medicion {
    Estado estado = NO_EJECUTADO;
    double ms = 0.0;                        // tiempo promedio por corrida
    int repeticiones = 0;
    set<set<int>> tipicos;                  // testores tipicos (si termino)
    unsigned long long hits = 0;            // YYC: candidatos comprobados
                                            // BT : listas examinadas
    unsigned long long maxIntermedios = 0;  // solo YYC
};

// Las cuatro corridas que se hacen sobre cada matriz.
const int YYC_ORIGINAL = 0, YYC_ASCENDENTE = 1, BT_ORIGINAL = 2, BT_ASCENDENTE = 3;
const char* const NOMBRE_CORRIDA[4] = {"YYC original", "YYC ascendente",
                                       "BT original", "BT ascendente"};

Reloj::time_point calcularLimite() {
    return Reloj::now() + chrono::duration_cast<Reloj::duration>(
                              chrono::duration<double>(LIMITE_SEGUNDOS));
}

// Pasa los testores de BT (mascaras) a conjuntos de columnas, la misma
// representacion de YYC, para poder comparar los resultados.
set<set<int>> mascarasAConjuntos(const vector<Mascara>& listas, int n) {
    set<set<int>> r;
    for (Mascara m : listas) {
        set<int> testor;
        for (int j = 0; j < n; ++j)
            if ((m >> (n - 1 - j)) & 1ULL) testor.insert(j);
        r.insert(testor);
    }
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

    med.estado = r.completo ? COMPLETO : CORTADO;
    med.ms = totalMs / med.repeticiones;
    med.hits = r.hits;
    med.maxIntermedios = r.maximoIntermedio;
    if (r.completo) med.tipicos = r.tipicos;
    return med;
}

Medicion medirBT(const Matriz& M) {
    const int n = static_cast<int>(M[0].size());
    Medicion med;
    if (n > MAX_COLUMNAS_BT) return med;     // BT no admite esta matriz

    vector<Mascara> MB;
    for (const auto& fila : M) MB.push_back(filaAMascara(fila));

    double totalMs = 0.0;
    ResultadoBT r;
    do {
        auto inicio = Reloj::now();
        r = ejecutarBT(MB, n, calcularLimite());
        totalMs += chrono::duration<double, milli>(Reloj::now() - inicio).count();
        ++med.repeticiones;
    } while (r.completo && totalMs < TIEMPO_MINIMO_MS &&
             med.repeticiones < MAX_REPETICIONES);

    med.estado = r.completo ? COMPLETO : CORTADO;
    med.ms = totalMs / med.repeticiones;
    med.hits = static_cast<unsigned long long>(r.examinadas);
    if (r.completo) med.tipicos = mascarasAConjuntos(r.tipicos, n);
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
// 5.2  FILAS DE LAS TABLAS Y VERIFICACION
// ============================================================================

enum Verificacion {
    VERIFICADO,     // las cuatro corridas terminaron y coinciden
    PARCIAL,        // coinciden las corridas que terminaron, pero no todas terminaron
    SIN_VERIFICAR,  // ninguna corrida termino: no hay nada que comparar
    NO_COINCIDE     // hay resultados distintos
};

struct FilaTabla {
    string nombre;        // consola:  "phi^2(theta(A,B))"
    string nombreCsv;     // CSV:      "phi^2(thetaAB)"   (sin comas)
    string nombreLatex;   // reporte:  "$\varphi^{2}(\theta(A,B))$"
    int op = 0;           // 1 = phi, 2 = theta, 3 = gamma
    int N = 0;
    size_t filas = 0;
    size_t columnas = 0;
    double densidad = 0.0;
    unsigned long long tt = 0;      // testores tipicos segun las formulas
    Medicion med[4];                // YYC original, YYC ascendente, BT original, BT ascendente
    Verificacion verificacion = SIN_VERIFICAR;
};

// Compara los resultados de las corridas que terminaron. Cada una debe
// tener la cantidad de testores tipicos que predicen las formulas, y todas
// deben devolver exactamente el mismo conjunto.
Verificacion verificar(const FilaTabla& f) {
    int completas = 0;
    const Medicion* referencia = nullptr;
    for (const Medicion& m : f.med) {
        if (m.estado != COMPLETO) continue;
        ++completas;
        if (m.tipicos.size() != f.tt) return NO_COINCIDE;
        if (referencia == nullptr) referencia = &m;
        else if (m.tipicos != referencia->tipicos) return NO_COINCIDE;
    }
    if (completas == 0) return SIN_VERIFICAR;
    return completas == 4 ? VERIFICADO : PARCIAL;
}

const char* textoVerificacion(Verificacion v) {
    switch (v) {
        case VERIFICADO:    return "si";
        case PARCIAL:       return "parcial";
        case SIN_VERIFICAR: return "sin datos";
        default:            return "NO";
    }
}

// Datos de la matriz O^N(base) que no dependen de las corridas.
FilaTabla prepararFila(int op, int N, const Base& base, const vector<size_t>& tamanos,
                       const Matriz& generada) {
    FilaTabla f;
    const string n = to_string(N);
    f.nombre = string(NOMBRE_OP[op]) + "^" + n + "(" + base.nombre + ")";
    f.nombreCsv = string(NOMBRE_OP[op]) + "^" + n + "(" + base.nombreArchivo + ")";
    f.nombreLatex = string("$") + LATEX_OP[op] + "^{" + n + "}(" + base.nombreLatex + ")$";
    f.op = op + 1;
    f.N = N;
    f.filas = generada.size();
    f.columnas = generada[0].size();
    f.densidad = calcularDensidad(generada);
    f.tt = ttTeorico(op, N, tamanos);
    return f;
}

// Numero para el reporte en LaTeX, con coma decimal: 0.4082 -> $0{,}4082$
string numeroLatex(double valor, int decimales) {
    ostringstream s;
    s << fixed << setprecision(decimales) << valor;
    string texto = s.str();
    const size_t punto = texto.find('.');
    if (punto != string::npos) texto.replace(punto, 1, "{,}");
    return "$" + texto + "$";
}

// Tiempo de una corrida para la consola (latex = false) o para el reporte.
string textoTiempo(const Medicion& m, bool latex) {
    ostringstream s;
    if (m.estado == NO_EJECUTADO) {
        s << (latex ? "---" : "--");
    } else if (m.estado == CORTADO) {
        if (latex) s << "$>" << fixed << setprecision(0) << LIMITE_SEGUNDOS << "$ s";
        else s << ">" << fixed << setprecision(0) << LIMITE_SEGUNDOS << " s";
    } else if (latex) {
        s << numeroLatex(m.ms, 4);
    } else {
        s << fixed << setprecision(4) << m.ms;
    }
    return s.str();
}

// Hits de una corrida. Solo se conocen si la corrida termino.
string textoHits(const Medicion& m, bool latex) {
    if (m.estado != COMPLETO) return latex ? "---" : "--";
    return to_string(m.hits);
}

// Eficiencia como en los articulos: testores tipicos / hits, en porcentaje.
// Se muestran dos decimales, o dos cifras significativas si es menor que 1.
string textoEficiencia(unsigned long long tt, const Medicion& m, bool latex) {
    if (m.estado != COMPLETO || m.hits == 0) return latex ? "---" : "--";
    const double e = 100.0 * static_cast<double>(tt) / static_cast<double>(m.hits);
    int decimales = 2;
    if (e < 1.0) {
        decimales = 1;
        for (double x = e; x < 1.0 && decimales < 9; x *= 10.0) ++decimales;
    }
    if (latex) return numeroLatex(e, decimales);
    ostringstream s;
    s << fixed << setprecision(decimales) << e;
    return s.str();
}

// Valor numerico para el CSV: "nan" si la corrida no termino, para que el
// archivo siga siendo numerico (lo leen los graficos del reporte).
string tiempoCsv(const Medicion& m) {
    return m.estado == COMPLETO ? textoTiempo(m, false) : string("nan");
}
string hitsCsv(const Medicion& m) {
    return m.estado == COMPLETO ? to_string(m.hits) : string("nan");
}

// Genera O^N(base), hace las cuatro corridas y verifica los resultados.
// omitir[c] = true indica que la corrida c no debe ejecutarse (el algoritmo
// ya supero el limite con un N menor de la misma familia).
FilaTabla procesarMatriz(int op, int N, const Base& base, const vector<size_t>& tamanos,
                         const bool omitir[4], Matriz* matrizGenerada) {
    const Matriz original = potencia(OPERADORES[op], base.M, N);
    const Matriz ascendente = ordenarAscendente(original);
    if (matrizGenerada != nullptr) *matrizGenerada = original;

    FilaTabla f = prepararFila(op, N, base, tamanos, original);
    cout << "  " << f.nombre << " (" << f.filas << " x " << f.columnas << "):" << flush;
    for (int c = 0; c < 4; ++c) {
        if (omitir != nullptr && omitir[c]) {
            cout << " [" << NOMBRE_CORRIDA[c] << ": omitida]" << flush;
            continue;                         // queda como NO_EJECUTADO
        }
        const Matriz& M = (c == YYC_ORIGINAL || c == BT_ORIGINAL) ? original : ascendente;
        f.med[c] = (c == YYC_ORIGINAL || c == YYC_ASCENDENTE) ? medirYYC(M) : medirBT(M);
        cout << " [" << NOMBRE_CORRIDA[c] << ": " << textoTiempo(f.med[c], false) << "]" << flush;
    }
    cout << "\n";
    f.verificacion = verificar(f);
    return f;
}

// ============================================================================
// SALIDA: CONSOLA, CSV Y LATEX
// ============================================================================

void imprimirEncabezado() {
    cout << right << setw(3) << "N" << "  " << left << setw(21) << "Matriz" << right
         << setw(7) << "Filas" << setw(6) << "Cols" << setw(8) << "TT"
         << setw(13) << "YYC orig" << setw(13) << "YYC asc" << setw(13) << "BT orig"
         << setw(13) << "BT asc" << setw(11) << "Verif." << "\n";
    cout << string(110, '-') << "\n";
}

void imprimirFila(const FilaTabla& f) {
    cout << right << setw(3) << f.N << "  " << left << setw(21) << f.nombre << right
         << setw(7) << f.filas << setw(6) << f.columnas << setw(8) << f.tt;
    for (const Medicion& m : f.med) cout << setw(13) << textoTiempo(m, false);
    cout << setw(11) << textoVerificacion(f.verificacion) << "\n";
}

void imprimirHits(const vector<FilaTabla>& tabla) {
    cout << "\nHits (YYC: candidatos comprobados; BT: listas examinadas) y eficiencia = TT / hits:\n";
    cout << right << setw(3) << "N" << "  " << left << setw(21) << "Matriz" << right
         << setw(13) << "YYC orig" << setw(10) << "efic. %" << setw(13) << "YYC asc"
         << setw(10) << "efic. %" << setw(13) << "BT orig" << setw(10) << "efic. %"
         << setw(13) << "BT asc" << setw(10) << "efic. %" << "\n";
    cout << string(118, '-') << "\n";
    for (const FilaTabla& f : tabla) {
        cout << right << setw(3) << f.N << "  " << left << setw(21) << f.nombre << right;
        for (const Medicion& m : f.med)
            cout << setw(13) << textoHits(m, false) << setw(10) << textoEficiencia(f.tt, m, false);
        cout << "\n";
    }
}

void guardarCsv(const vector<FilaTabla>& tabla, const string& ruta) {
    ofstream csv(ruta);
    if (!csv) {
        cerr << "No se pudo abrir " << ruta << " para escritura.\n";
        return;
    }
    // op: 1 = phi, 2 = theta, 3 = gamma. "nan": la corrida no termino.
    csv << "matriz,op,n,filas,columnas,densidad,tt,"
        << "yyc_original_ms,yyc_ascendente_ms,bt_original_ms,bt_ascendente_ms,"
        << "yyc_hits_original,yyc_hits_ascendente,bt_hits_original,bt_hits_ascendente,"
        << "yyc_max_intermedios_original,yyc_max_intermedios_ascendente,verificacion\n";
    for (const FilaTabla& f : tabla) {
        csv << f.nombreCsv << ',' << f.op << ',' << f.N << ',' << f.filas << ','
            << f.columnas << ',' << fixed << setprecision(4) << f.densidad << ','
            << f.tt;
        for (const Medicion& m : f.med) csv << ',' << tiempoCsv(m);
        for (const Medicion& m : f.med) csv << ',' << hitsCsv(m);
        for (int c = YYC_ORIGINAL; c <= YYC_ASCENDENTE; ++c) {
            if (f.med[c].estado == COMPLETO) csv << ',' << f.med[c].maxIntermedios;
            else csv << ",nan";
        }
        csv << ',' << textoVerificacion(f.verificacion) << '\n';
    }
    cout << "Datos guardados en " << ruta << ".\n";
}

bool hayEstado(const vector<FilaTabla>& tabla, Estado estado) {
    for (const FilaTabla& f : tabla)
        for (const Medicion& m : f.med)
            if (m.estado == estado) return true;
    return false;
}

// Tabla de tiempos lista para incluir en el reporte con \input{...}
// (requiere el paquete booktabs). Los numeros llevan coma decimal.
void guardarLatexTiempos(const vector<FilaTabla>& tabla, const string& ruta,
                         const string& descripcion, const string& etiqueta) {
    ofstream tex(ruta);
    if (!tex) {
        cerr << "No se pudo abrir " << ruta << " para escritura.\n";
        return;
    }
    tex << "\\begin{table}[htbp]\n\\centering\n\\small\n";
    tex << "\\caption{Matrices " << descripcion << " y tiempos de ejecuci\\'on (ms) de "
        << "YYC y BT con las filas en orden original y en orden ascendente.";
    if (hayEstado(tabla, CORTADO))
        tex << " ``$>" << fixed << setprecision(0) << LIMITE_SEGUNDOS
            << "$ s'': la corrida se interrumpi\\'o al superar el l\\'imite de tiempo.";
    if (hayEstado(tabla, NO_EJECUTADO))
        tex << " ``---'': la corrida no se ejecut\\'o.";
    tex << "}\n\\label{" << etiqueta << "}\n";
    tex << "\\begin{tabular}{rlrrrrrrr}\n\\toprule\n";
    tex << " & & & & & \\multicolumn{2}{c}{YYC} & \\multicolumn{2}{c}{BT} \\\\\n";
    tex << "\\cmidrule(lr){6-7}\\cmidrule(lr){8-9}\n";
    tex << "$N$ & Matriz & Filas & Col. & TT & Original & Ascendente "
        << "& Original & Ascendente \\\\\n\\midrule\n";
    int operadorAnterior = 0;
    for (const FilaTabla& f : tabla) {
        if (operadorAnterior != 0 && f.op != operadorAnterior) tex << "\\midrule\n";
        operadorAnterior = f.op;
        tex << f.N << " & " << f.nombreLatex << " & " << f.filas << " & " << f.columnas
            << " & " << f.tt;
        for (const Medicion& m : f.med) tex << " & " << textoTiempo(m, true);
        tex << " \\\\\n";
    }
    tex << "\\bottomrule\n\\end{tabular}\n\\end{table}\n";
    cout << "Tabla de tiempos guardada en " << ruta << ".\n";
}

// Tabla de hits y eficiencia, la medida que usan los articulos.
void guardarLatexHits(const vector<FilaTabla>& tabla, const string& ruta,
                      const string& descripcion, const string& etiqueta) {
    ofstream tex(ruta);
    if (!tex) {
        cerr << "No se pudo abrir " << ruta << " para escritura.\n";
        return;
    }
    // BT se muestra una sola vez si examino las mismas listas en ambos ordenes.
    bool btIgual = true;
    for (const FilaTabla& f : tabla) {
        const Medicion& o = f.med[BT_ORIGINAL];
        const Medicion& a = f.med[BT_ASCENDENTE];
        if (o.estado == COMPLETO && a.estado == COMPLETO && o.hits != a.hits) btIgual = false;
    }
    tex << "\\begin{table}[htbp]\n\\centering\n\\small\n";
    tex << "\\caption{Hits y eficiencia (testores t\\'ipicos entre hits, en \\%) de YYC "
        << "y BT sobre las matrices " << descripcion << ". En YYC un hit es un candidato "
        << "comprobado con \\texttt{FindCompatibleSet}; en BT, una lista examinada. "
        << (btIgual ? "BT examin\\'o las mismas listas con los dos \\'ordenes de filas."
                    : "Para BT se muestra el orden original.")
        << " ``---'': la corrida no termin\\'o o no se ejecut\\'o.}\n";
    tex << "\\label{" << etiqueta << "}\n";
    tex << "\\begin{tabular}{rlrrrrrrr}\n\\toprule\n";
    tex << " & & & \\multicolumn{2}{c}{YYC original} & \\multicolumn{2}{c}{YYC ascendente} "
        << "& \\multicolumn{2}{c}{BT} \\\\\n";
    tex << "\\cmidrule(lr){4-5}\\cmidrule(lr){6-7}\\cmidrule(lr){8-9}\n";
    tex << "$N$ & Matriz & TT & Hits & Efic. & Hits & Efic. & Hits & Efic. \\\\\n\\midrule\n";
    int operadorAnterior = 0;
    for (const FilaTabla& f : tabla) {
        if (operadorAnterior != 0 && f.op != operadorAnterior) tex << "\\midrule\n";
        operadorAnterior = f.op;
        const Medicion& bt = (f.med[BT_ORIGINAL].estado == COMPLETO) ? f.med[BT_ORIGINAL]
                                                                    : f.med[BT_ASCENDENTE];
        tex << f.N << " & " << f.nombreLatex << " & " << f.tt
            << " & " << textoHits(f.med[YYC_ORIGINAL], true)
            << " & " << textoEficiencia(f.tt, f.med[YYC_ORIGINAL], true)
            << " & " << textoHits(f.med[YYC_ASCENDENTE], true)
            << " & " << textoEficiencia(f.tt, f.med[YYC_ASCENDENTE], true)
            << " & " << textoHits(bt, true)
            << " & " << textoEficiencia(f.tt, bt, true) << " \\\\\n";
    }
    tex << "\\bottomrule\n\\end{tabular}\n\\end{table}\n";
    cout << "Tabla de hits guardada en " << ruta << ".\n";
}

// ============================================================================
// EXPERIMENTOS
// ============================================================================

// Una familia es un operador aplicado N = 1..nMax veces sobre una matriz base.
struct Familia {
    int op;              // 0 = phi, 1 = theta, 2 = gamma
    int nMax;
    const Base* base;
};

struct Experimento {
    string id;           // los archivos se llaman tabla_ej5_<id>.csv / .tex
    string titulo;       // para la consola
    string descripcion;  // para los titulos de las tablas del reporte (LaTeX)
    vector<Familia> familias;
};

void imprimirResumenVerificacion(const vector<FilaTabla>& tabla) {
    int cuenta[4] = {0, 0, 0, 0};
    for (const FilaTabla& f : tabla) ++cuenta[f.verificacion];
    cout << "\nVerificacion (mismo conjunto de testores tipicos en las corridas que "
         << "terminaron,\ny cantidad igual a la que predicen las formulas del articulo):\n";
    const size_t total = tabla.size();
    cout << "  completa (las cuatro corridas): " << cuenta[VERIFICADO] << " de " << total << "\n";
    if (cuenta[PARCIAL] > 0)
        cout << "  parcial (solo las corridas que terminaron): " << cuenta[PARCIAL]
             << " de " << total << "\n";
    if (cuenta[SIN_VERIFICAR] > 0)
        cout << "  sin verificar (ninguna corrida termino): " << cuenta[SIN_VERIFICAR]
             << " de " << total << "\n";
    if (cuenta[NO_COINCIDE] > 0)
        cout << "  ATENCION: resultados distintos en " << cuenta[NO_COINCIDE]
             << " de " << total << "\n";
}

void imprimirLeyenda(const vector<FilaTabla>& tabla) {
    if (hayEstado(tabla, CORTADO))
        cout << ">" << fixed << setprecision(0) << LIMITE_SEGUNDOS
             << " s : la corrida se interrumpio al superar el limite de tiempo.\n";
    if (hayEstado(tabla, NO_EJECUTADO))
        cout << "--     : no se ejecuto, porque el algoritmo ya habia superado el limite con un N\n"
             << "         menor de la misma familia o porque BT admite hasta " << MAX_COLUMNAS_BT
             << " columnas.\n";
}

// Muestra las tablas de un experimento y las guarda en CSV y LaTeX.
void presentarResultados(const Experimento& e, const vector<FilaTabla>& tabla) {
    cout << "\n";
    imprimirEncabezado();
    for (const FilaTabla& f : tabla) imprimirFila(f);
    cout << string(110, '-') << "\n";
    imprimirLeyenda(tabla);
    imprimirResumenVerificacion(tabla);
    imprimirHits(tabla);

    cout << "\n";
    const string prefijo = "tabla_ej5_" + e.id;
    guardarCsv(tabla, prefijo + ".csv");
    guardarLatexTiempos(tabla, prefijo + ".tex", e.descripcion, "tab:ej5" + e.id);
    guardarLatexHits(tabla, prefijo + "_hits.tex", e.descripcion, "tab:ej5" + e.id + "hits");
}

// Mide todas las matrices de un experimento y presenta los resultados.
void ejecutarExperimento(const Experimento& e) {
    cout << "\n==================== " << e.titulo << " ====================\n";
    cout << "Tiempos en milisegundos. Limite por corrida: " << fixed << setprecision(0)
         << LIMITE_SEGUNDOS << " s.\n";
    cout << "Avance (cada corrida puede tardar hasta el limite):\n";

    vector<FilaTabla> tabla;
    for (const Familia& fam : e.familias) {
        const vector<size_t> tamanos = tamanosTestoresTipicos(fam.base->M);
        bool omitir[4] = {false, false, false, false};
        for (int N = 1; N <= fam.nMax; ++N) {
            tabla.push_back(procesarMatriz(fam.op, N, *fam.base, tamanos, omitir, nullptr));
            // Un algoritmo que supero el limite no se repite con matrices mayores.
            for (int c = 0; c < 4; ++c)
                if (tabla.back().med[c].estado == CORTADO) omitir[c] = true;
        }
    }

    presentarResultados(e, tabla);
}

// Una sola matriz, para demostrar la ejecucion y los tiempos.
void ejecutarUnaMatriz(const Base* bases[3]) {
    int b;
    cout << "Matriz base (1 = A, 2 = B, 3 = theta(A,B)): ";
    if (!(cin >> b) || b < 1 || b > 3) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "La matriz base debe ser 1, 2 o 3.\n";
        return;
    }
    const Base& base = *bases[b - 1];

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

    const vector<size_t> tamanos = tamanosTestoresTipicos(base.M);
    unsigned long long filas, columnas;
    dimensiones(op, N, base.M, filas, columnas);
    if (columnas > MAX_COLUMNAS || filas > MAX_FILAS || ttTeorico(op, N, tamanos) > MAX_TT) {
        cout << "La matriz " << NOMBRE_OP[op] << "^" << N << "(" << base.nombre
             << ") tendria " << filas << " filas, " << columnas << " columnas y "
             << ttTeorico(op, N, tamanos) << " testores tipicos.\n"
             << "Supera los limites de esta implementacion (" << MAX_COLUMNAS
             << " columnas, " << MAX_FILAS << " filas, " << MAX_TT << " testores).\n";
        return;
    }

    cout << "\nGenerando y midiendo (cada corrida puede tardar hasta " << fixed
         << setprecision(0) << LIMITE_SEGUNDOS << " s):\n";
    Matriz generada;
    FilaTabla f = procesarMatriz(op, N, base, tamanos, nullptr, &generada);

    if (generada.size() <= 16 && generada[0].size() <= 28) {
        cout << "\nMatriz generada:\n";
        imprimirMatriz(cout, generada);
    }
    cout << "\n";
    imprimirEncabezado();
    imprimirFila(f);
    const vector<FilaTabla> una(1, f);
    imprimirLeyenda(una);
    cout << "\nRepeticiones promediadas:";
    for (int c = 0; c < 4; ++c)
        if (f.med[c].estado != NO_EJECUTADO)
            cout << " " << NOMBRE_CORRIDA[c] << " " << f.med[c].repeticiones << ";";
    cout << "\n";
    imprimirResumenVerificacion(una);
    imprimirHits(una);

    // Se guarda con el mismo formato del Ejercicio 1, para poder cargarla en
    // los programas de los Ejercicios 2 y 3 (opcion "leer desde archivo").
    const string ruta = string(NOMBRE_OP[op]) + to_string(N) + "_" + base.nombreArchivo + ".txt";
    ofstream archivo(ruta);
    if (archivo) {
        archivo << generada.size() << ' ' << generada[0].size() << '\n';
        imprimirMatriz(archivo, generada);
        cout << "\nMatriz guardada en " << ruta << ".\n";
    }
}

// ============================================================================
// VALIDACION CON LOS HITS PUBLICADOS EN LOS ARTICULOS
// Se ejecutan YYC y BT sobre matrices de los propios articulos y se comparan
// los hits obtenidos con los publicados (Tablas 1 y 2 del articulo de YYC,
// que usan las matrices A y B del Ejemplo 1 del articulo de 2016).
// ============================================================================

const Matriz ARTICULO_A = {
    {1, 0, 0, 0, 0},
    {0, 0, 1, 0, 1},
    {0, 1, 0, 1, 1},
    {0, 1, 1, 0, 0}
};
const Matriz ARTICULO_B = {
    {1, 1, 1, 0, 0},
    {1, 1, 0, 0, 1},
    {1, 0, 1, 1, 0},
    {1, 0, 1, 0, 1}
};

Matriz matrizIdentidad(int n) {
    Matriz I(static_cast<size_t>(n), vector<int>(static_cast<size_t>(n), 0));
    for (int i = 0; i < n; ++i) I[static_cast<size_t>(i)][static_cast<size_t>(i)] = 1;
    return I;
}

// Una fila de la comparacion. conBT = false omite BT (tardaria demasiado).
void compararConPublicado(const string& nombre, const Matriz& M, unsigned long long ttPublicado,
                          unsigned long long publicadoYYC, unsigned long long publicadoBT,
                          bool conBT) {
    const ResultadoYYC y = ejecutarYYC(M, Reloj::time_point::max());
    cout << left << setw(26) << nombre << right << setw(6) << M.size() << setw(6)
         << M[0].size() << setw(8) << ttPublicado << setw(8) << y.tipicos.size()
         << setw(12) << publicadoYYC << setw(12) << y.hits
         << setw(5) << (y.hits == publicadoYYC ? "=" : "<>") << setw(12) << publicadoBT;
    if (conBT) {
        vector<Mascara> MB;
        for (const auto& fila : M) MB.push_back(filaAMascara(fila));
        const ResultadoBT b = ejecutarBT(MB, static_cast<int>(M[0].size()), calcularLimite());
        if (b.completo)
            cout << setw(12) << b.examinadas
                 << setw(5) << (static_cast<unsigned long long>(b.examinadas) == publicadoBT ? "=" : "<>");
        else
            cout << setw(12) << "> limite" << setw(5) << "";
    } else {
        cout << setw(12) << "--" << setw(5) << "";
    }
    cout << endl;
}

void validarConArticulo() {
    cout << "\nHits publicados en los articulos frente a los que obtienen estas implementaciones.\n"
         << "YYC: candidatos comprobados con FindCompatibleSet.  BT: listas examinadas.\n\n";
    cout << left << setw(26) << "Matriz" << right << setw(6) << "Filas" << setw(6) << "Cols"
         << setw(8) << "TT art." << setw(8) << "TT" << setw(12) << "YYC art." << setw(12)
         << "YYC" << setw(5) << "" << setw(12) << "BT art." << setw(12) << "BT" << "\n";
    cout << string(112, '-') << "\n";

    // Matrices identidad (Tabla 1 del articulo de YYC).
    const int tamanos[5] = {5, 10, 15, 20, 25};
    const unsigned long long yycIdentidad[5] = {4, 9, 14, 19, 24};
    const unsigned long long btIdentidad[5] = {6, 11, 16, 21, 26};
    for (int i = 0; i < 5; ++i)
        compararConPublicado("I" + to_string(tamanos[i]), matrizIdentidad(tamanos[i]), 1,
                             yycIdentidad[i], btIdentidad[i], true);
    cout << string(112, '-') << "\n";

    // phi^N(theta(A,B)) con las matrices A y B del articulo (Tabla 2 del
    // articulo de YYC y Tabla 5 del articulo de 2016).
    const Matriz T = opTheta(ARTICULO_A, ARTICULO_B);
    const unsigned long long ttPublicado[5] = {8, 50, 156, 356, 680};
    const unsigned long long yycPublicado[5] = {76, 1056, 5652, 19984, 54700};
    const unsigned long long btPublicado[5] = {94, 8861, 444459, 19762606, 75336732};
    for (int N = 1; N <= 5; ++N)
        compararConPublicado("phi^" + to_string(N) + "(theta(A,B)) art.", potencia(opPhi, T, N),
                             ttPublicado[N - 1], yycPublicado[N - 1], btPublicado[N - 1], N <= 4);
    cout << string(112, '-') << "\n";
    cout << "=  : coincide con el valor publicado.   <> : no coincide.   -- : no se ejecuto.\n"
         << "Las listas que examina BT dependen del orden de las columnas, que el articulo no indica.\n";
}

// Los cuatro experimentos del menu. bases = {A, B, theta(A,B)}.
vector<Experimento> definirExperimentos(const Base* bases[3]) {
    const Base* A = bases[0];
    const Base* B = bases[1];
    const Base* T = bases[2];
    vector<Experimento> e;
    // Tablas principales: el procedimiento de las Tablas 5 y 6 del articulo.
    e.push_back({"phi", "TABLA phi^N(theta(A,B))",
                 "$\\varphi^{N}(\\theta(A,B))$",
                 {{0, N_MAX_PHI_PRINCIPAL, T}}});
    e.push_back({"gamma", "TABLA gamma^N(theta(A,B))",
                 "$\\gamma^{N}(\\theta(A,B))$",
                 {{2, N_MAX_GAMMA_PRINCIPAL, T}}});
    // Experimento adicional: los tres operadores sobre A y sobre B.
    e.push_back({"A", "EXPERIMENTO ADICIONAL CON LA MATRIZ A",
                 "$\\varphi^{N}(A)$, $\\theta^{N}(A)$ y $\\gamma^{N}(A)$",
                 {{0, N_MAX_A[0], A}, {1, N_MAX_A[1], A}, {2, N_MAX_A[2], A}}});
    e.push_back({"B", "EXPERIMENTO ADICIONAL CON LA MATRIZ B",
                 "$\\varphi^{N}(B)$, $\\theta^{N}(B)$ y $\\gamma^{N}(B)$",
                 {{0, N_MAX_B[0], B}, {1, N_MAX_B[1], B}, {2, N_MAX_B[2], B}}});
    return e;
}

int main() {
    const Base baseA = {"A", "A", "A", MATRIZ_A};
    const Base baseB = {"B", "B", "B", MATRIZ_B};
    // theta(A,B): cada fila de A concatenada con cada fila de B (56 x 14).
    const Base baseT = {"theta(A,B)", "thetaAB", "\\theta(A,B)", opTheta(MATRIZ_A, MATRIZ_B)};
    const Base* bases[3] = {&baseA, &baseB, &baseT};

    const vector<Experimento> experimentos = definirExperimentos(bases);

    cout << "====================================================================\n";
    cout << "   OPERADORES phi, theta, gamma Y TIEMPOS DE YYC Y BT (EJ. 5)\n";
    cout << "====================================================================\n";

    while (true) {
        cout << "\n1. Tabla phi^N(theta(A,B))    (como la Tabla 5 del articulo)\n";
        cout << "2. Tabla gamma^N(theta(A,B))  (como la Tabla 6 del articulo)\n";
        cout << "3. Experimento adicional: phi^N, theta^N y gamma^N de A\n";
        cout << "4. Experimento adicional: phi^N, theta^N y gamma^N de B\n";
        cout << "5. Generar y medir una sola matriz\n";
        cout << "6. Comprobar los hits con las matrices de los articulos\n";
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
        if (opcion >= 1 && opcion <= 4) ejecutarExperimento(experimentos[opcion - 1]);
        else if (opcion == 5) ejecutarUnaMatriz(bases);
        else if (opcion == 6) validarConArticulo();
        else cout << "Opcion invalida.\n";
    }
    return 0;
}
