//
//  Ejercicio6.cpp
//  Algoritmo exhaustivo para testores tipicos y comparacion con YYC y BT.
//
//  6.1  ejecutarExhaustivo(): recorre los 2^n - 1 subconjuntos no vacios de
//       columnas, marca cuales son testores y despues decide cuales de ellos
//       son tipicos. No usa ninguna funcion de YYC ni de BT.
//  6.2  Lee las matrices basicas generadas con el programa del Ejercicio 1
//       (las copias numeradas matriz_basica_1.txt, matriz_basica_2.txt, ...),
//       calcula los testores tipicos con YYC, BT y el exhaustivo, comprueba
//       que los tres conjuntos son exactamente iguales y mide los tiempos.
//  6.3  Muestra la tabla comparativa y la guarda en tabla_ej6.csv y
//       tabla_ej6.tex.
//
//  YYC y BT son los mismos de Ejercicio2.cpp y Ejercicio3.cpp (BT sin la traza).
//
//  Compilar:  g++ -std=c++11 -O2 -o ej6 Ejercicio6.cpp
//

#include <iostream>
#include <vector>
#include <set>
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

using Matriz = vector<vector<int>>;
using Mascara = uint64_t;
using Reloj = chrono::steady_clock;

// ============================================================================
// PARAMETROS DEL EXPERIMENTO (se pueden cambiar aqui)
// ============================================================================

// Las matrices del experimento son las copias numeradas que guarda el
// programa del Ejercicio 1: PREFIJO1.txt, PREFIJO2.txt, ... Se procesan todas
// las que existan en la carpeta actual con numero entre 1 y MAX_MATRICES.
const string PREFIJO_ARCHIVO = "matriz_basica_";
const int MAX_MATRICES = 200;

// El exhaustivo revisa 2^n - 1 subconjuntos: con mas columnas que estas se
// vuelve demasiado lento y la tabla de testores no cabe en memoria.
const int MAX_COLUMNAS = 20;

// Las corridas se repiten hasta acumular este tiempo (o hasta el maximo de
// repeticiones) y se reporta el promedio. Se aplica igual a los tres algoritmos.
const double TIEMPO_MINIMO_MS = 200.0;
const int MAX_REPETICIONES = 1000;

// ============================================================================
// ENTRADA Y VALIDACION (copiadas de Ejercicio2.cpp / Ejercicio3.cpp)
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

// ============================================================================
// UTILIDADES
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

void imprimirMatriz(ostream& salida, const Matriz& M) {
    for (const auto& fila : M) {
        for (size_t j = 0; j < fila.size(); ++j) {
            if (j != 0) salida << ' ';
            salida << fila[j];
        }
        salida << '\n';
    }
}

void imprimirTestor(ostream& salida, const set<int>& testor) {
    salida << '{';
    for (auto it = testor.begin(); it != testor.end(); ++it) {
        if (it != testor.begin()) salida << ", ";
        salida << 'x' << (*it + 1);
    }
    salida << '}';
}

// ============================================================================
// 6.1  ALGORITMO EXHAUSTIVO
//
// Cada subconjunto de columnas se identifica con un numero s entre 1 y
// 2^n - 1: la columna x(j+1) pertenece al subconjunto si el bit j de s vale 1.
//
// Fase 1: se recorren TODOS los subconjuntos y se anota cuales son testores.
// Fase 2: para cada testor se comprueba la tipicidad.
// ============================================================================

// Un subconjunto es testor si ninguna fila queda en ceros al mirar solamente
// sus columnas.
bool cubreTodasLasFilas(const Matriz& M, unsigned long long s) {
    const int n = static_cast<int>(M[0].size());
    for (const auto& fila : M) {
        bool tieneUno = false;
        for (int j = 0; j < n; ++j) {
            if (((s >> j) & 1ULL) && fila[j] == 1) {
                tieneUno = true;
                break;
            }
        }
        if (!tieneUno) return false;   // esta fila quedo en ceros
    }
    return true;
}

struct ResultadoExhaustivo {
    vector<vector<int>> tipicos;             // columnas (0-based) de cada testor tipico
    unsigned long long subconjuntos = 0;     // subconjuntos revisados (2^n - 1)
    unsigned long long testores = 0;         // cuantos de ellos son testores
};

ResultadoExhaustivo ejecutarExhaustivo(const Matriz& M) {
    const int n = static_cast<int>(M[0].size());
    const unsigned long long total = 1ULL << n;   // 2^n
    ResultadoExhaustivo r;

    // Fase 1: tabla con la respuesta "es testor?" de cada subconjunto.
    vector<char> esTestorTabla(total, 0);
    for (unsigned long long s = 1; s < total; ++s) {
        ++r.subconjuntos;
        if (cubreTodasLasFilas(M, s)) {
            esTestorTabla[s] = 1;
            ++r.testores;
        }
    }

    // Fase 2: un testor es tipico si al quitarle una columna, cualquiera que
    // sea, deja de ser testor. Basta quitar las columnas de una en una: si
    // un subconjunto propio mas pequeno fuera testor, tambien lo seria el que
    // se obtiene quitando solo una de las columnas que le faltan.
    for (unsigned long long s = 1; s < total; ++s) {
        if (!esTestorTabla[s]) continue;
        bool tipico = true;
        for (int j = 0; j < n && tipico; ++j) {
            if ((s >> j) & 1ULL) {
                const unsigned long long sinColumna = s & ~(1ULL << j);
                if (esTestorTabla[sinColumna]) tipico = false;
            }
        }
        if (tipico) {
            vector<int> columnas;
            for (int j = 0; j < n; ++j)
                if ((s >> j) & 1ULL) columnas.push_back(j);
            r.tipicos.push_back(columnas);
        }
    }
    return r;
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

// Mismo recorrido fila por fila de ejecutarAlgoritmoYYC (Ej. 2), sin imprimir.
set<set<int>> ejecutarYYC(const Matriz& BM) {
    set<set<int>> psiStar;
    for (int col = 0; col < static_cast<int>(BM[0].size()); ++col) {
        if (BM[0][col] == 1) psiStar.insert({col});
    }
    for (int fila = 1; fila < static_cast<int>(BM.size()); ++fila) {
        psiStar = actualizarTestores(BM, fila, psiStar);
    }
    return psiStar;
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
};

ResultadoBT ejecutarBT(const vector<Mascara>& MB, int n) {
    ResultadoBT r;
    const Mascara ultimo = (1ULL << n) - 1;
    Mascara alfa = 1;

    while (alfa <= ultimo) {
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
// 6.2  EJECUCION DE LOS TRES ALGORITMOS, TIEMPOS Y VERIFICACION
// ============================================================================

struct FilaTabla {
    string nombre;                 // M1, M2, ...
    string archivo;
    size_t filas = 0;
    size_t columnas = 0;
    double densidad = 0.0;
    double msYYC = 0.0, msBT = 0.0, msExhaustivo = 0.0;
    set<set<int>> tipicosYYC, tipicosBT, tipicosExhaustivo;
    long long listasBT = 0;                  // listas examinadas por BT
    unsigned long long subconjuntos = 0;     // subconjuntos revisados por el exhaustivo
    unsigned long long testores = 0;         // testores (tipicos o no) de la matriz
    bool coinciden = false;                  // los tres conjuntos son iguales
};

// Los tres algoritmos se miden de la misma manera: se repite la corrida
// hasta acumular TIEMPO_MINIMO_MS y se devuelve el promedio en milisegundos.
double medirYYC(const Matriz& M, set<set<int>>& tipicos) {
    double totalMs = 0.0;
    int repeticiones = 0;
    do {
        auto inicio = Reloj::now();
        tipicos = ejecutarYYC(M);
        totalMs += chrono::duration<double, milli>(Reloj::now() - inicio).count();
        ++repeticiones;
    } while (totalMs < TIEMPO_MINIMO_MS && repeticiones < MAX_REPETICIONES);
    return totalMs / repeticiones;
}

double medirBT(const Matriz& M, ResultadoBT& resultado) {
    const int n = static_cast<int>(M[0].size());
    vector<Mascara> MB;
    for (const auto& fila : M) MB.push_back(filaAMascara(fila));

    double totalMs = 0.0;
    int repeticiones = 0;
    do {
        auto inicio = Reloj::now();
        resultado = ejecutarBT(MB, n);
        totalMs += chrono::duration<double, milli>(Reloj::now() - inicio).count();
        ++repeticiones;
    } while (totalMs < TIEMPO_MINIMO_MS && repeticiones < MAX_REPETICIONES);
    return totalMs / repeticiones;
}

double medirExhaustivo(const Matriz& M, ResultadoExhaustivo& resultado) {
    double totalMs = 0.0;
    int repeticiones = 0;
    do {
        auto inicio = Reloj::now();
        resultado = ejecutarExhaustivo(M);
        totalMs += chrono::duration<double, milli>(Reloj::now() - inicio).count();
        ++repeticiones;
    } while (totalMs < TIEMPO_MINIMO_MS && repeticiones < MAX_REPETICIONES);
    return totalMs / repeticiones;
}

// Calcula los testores tipicos de M con los tres algoritmos, mide los tiempos
// y comprueba que los tres conjuntos sean exactamente iguales.
FilaTabla procesarMatriz(const Matriz& M, const string& nombre, const string& archivo) {
    FilaTabla f;
    f.nombre = nombre;
    f.archivo = archivo;
    f.filas = M.size();
    f.columnas = M[0].size();
    f.densidad = calcularDensidad(M);
    const int n = static_cast<int>(f.columnas);

    f.msYYC = medirYYC(M, f.tipicosYYC);

    ResultadoBT bt;
    f.msBT = medirBT(M, bt);
    f.listasBT = bt.examinadas;
    for (Mascara m : bt.tipicos) {           // mascara -> conjunto de columnas
        set<int> testor;
        for (int j = 0; j < n; ++j)
            if ((m >> (n - 1 - j)) & 1ULL) testor.insert(j);
        f.tipicosBT.insert(testor);
    }

    ResultadoExhaustivo ex;
    f.msExhaustivo = medirExhaustivo(M, ex);
    f.subconjuntos = ex.subconjuntos;
    f.testores = ex.testores;
    for (const auto& columnas : ex.tipicos)
        f.tipicosExhaustivo.insert(set<int>(columnas.begin(), columnas.end()));

    // Verificacion: los tres conjuntos deben ser exactamente el mismo.
    f.coinciden = (f.tipicosYYC == f.tipicosBT) && (f.tipicosBT == f.tipicosExhaustivo);
    return f;
}

// ============================================================================
// 6.3  TABLA COMPARATIVA
// ============================================================================

void imprimirEncabezado() {
    cout << left << setw(8) << "Matriz" << right << setw(6) << "Filas" << setw(6)
         << "Cols" << setw(10) << "Densidad" << setw(6) << "TT" << setw(12)
         << "YYC (ms)" << setw(12) << "BT (ms)" << setw(12) << "Exh. (ms)"
         << setw(11) << "Listas BT" << setw(11) << "Subc. exh" << setw(11)
         << "Coinciden" << "\n";
    cout << string(105, '-') << "\n";
}

void imprimirFila(const FilaTabla& f) {
    cout << left << setw(8) << f.nombre << right << setw(6) << f.filas << setw(6)
         << f.columnas << setw(10) << fixed << setprecision(4) << f.densidad
         << setw(6) << f.tipicosExhaustivo.size()
         << setw(12) << f.msYYC << setw(12) << f.msBT << setw(12) << f.msExhaustivo
         << setw(11) << f.listasBT << setw(11) << f.subconjuntos
         << setw(11) << (f.coinciden ? "si" : "NO") << endl;
}

void guardarCsv(const vector<FilaTabla>& tabla, const string& ruta) {
    ofstream csv(ruta);
    if (!csv) {
        cerr << "No se pudo abrir " << ruta << " para escritura.\n";
        return;
    }
    csv << "matriz,archivo,filas,columnas,densidad,tt,yyc_ms,bt_ms,exhaustivo_ms,"
        << "bt_listas_examinadas,exhaustivo_subconjuntos,testores_totales,"
        << "resultados_coinciden\n";
    for (const FilaTabla& f : tabla) {
        csv << f.nombre << ',' << f.archivo << ',' << f.filas << ',' << f.columnas << ','
            << fixed << setprecision(4) << f.densidad << ','
            << f.tipicosExhaustivo.size() << ',' << f.msYYC << ',' << f.msBT << ','
            << f.msExhaustivo << ',' << f.listasBT << ',' << f.subconjuntos << ','
            << f.testores << ',' << (f.coinciden ? "si" : "no") << '\n';
    }
    cout << "Tabla guardada en " << ruta << ".\n";
}

// Tabla lista para incluir en el reporte con \input{tabla_ej6}
void guardarLatex(const vector<FilaTabla>& tabla, const string& ruta) {
    ofstream tex(ruta);
    if (!tex) {
        cerr << "No se pudo abrir " << ruta << " para escritura.\n";
        return;
    }
    tex << "\\begin{table}[htbp]\n\\centering\n\\small\n";
    tex << "\\caption{Comparaci\\'on de YYC, BT y el algoritmo exhaustivo "
        << "(tiempos en ms).}\n";
    tex << "\\label{tab:ej6}\n";
    tex << "\\begin{tabular}{lrrrrrrrrrc}\n\\hline\n";
    tex << "Matriz & Filas & Col. & Densidad & TT & YYC & BT & Exhaustivo "
        << "& Listas BT & Subconj. & Iguales \\\\\n\\hline\n";
    for (size_t i = 0; i < tabla.size(); ++i) {
        const FilaTabla& f = tabla[i];
        tex << "$M_{" << (i + 1) << "}$ & " << f.filas << " & " << f.columnas << " & "
            << fixed << setprecision(4) << f.densidad << " & "
            << f.tipicosExhaustivo.size() << " & " << f.msYYC << " & " << f.msBT
            << " & " << f.msExhaustivo << " & " << f.listasBT << " & "
            << f.subconjuntos << " & " << (f.coinciden ? "S\\'i" : "No") << " \\\\\n";
    }
    tex << "\\hline\n\\end{tabular}\n\\end{table}\n";
    cout << "Tabla LaTeX guardada en " << ruta << ".\n";
}

string nombreArchivo(int numero) {
    ostringstream s;
    s << PREFIJO_ARCHIVO << numero << ".txt";
    return s.str();
}

// Lee una matriz basica de un archivo y comprueba que se pueda procesar.
bool cargarMatriz(const string& ruta, Matriz& M) {
    ifstream entrada(ruta);
    if (!entrada || !leerMatriz(entrada, M)) {
        cout << "No se pudo leer una matriz booleana en " << ruta << ".\n";
        return false;
    }
    if (!esMatrizBasica(M)) {
        cout << ruta << ": se requiere una matriz basica no vacia, sin filas nulas, "
             << "duplicadas ni superfilas.\n";
        return false;
    }
    if (static_cast<int>(M[0].size()) > MAX_COLUMNAS) {
        cout << ruta << ": tiene " << M[0].size() << " columnas y el exhaustivo admite "
             << "hasta " << MAX_COLUMNAS << ".\n";
        return false;
    }
    return true;
}

// Experimento completo sobre las copias numeradas matriz_basica_N.txt
void ejecutarExperimento() {
    vector<FilaTabla> tabla;
    cout << "\nTiempos en milisegundos (promedio de corridas repetidas).\n\n";
    imprimirEncabezado();
    for (int numero = 1; numero <= MAX_MATRICES; ++numero) {
        const string ruta = nombreArchivo(numero);
        if (!ifstream(ruta)) continue;       // ese numero no existe
        Matriz M;
        if (!cargarMatriz(ruta, M)) continue;
        tabla.push_back(procesarMatriz(M, "M" + to_string(tabla.size() + 1), ruta));
        imprimirFila(tabla.back());
    }
    cout << string(105, '-') << "\n";

    if (tabla.empty()) {
        cout << "No se encontro ninguna matriz " << PREFIJO_ARCHIVO
             << "N.txt en la carpeta actual.\n"
             << "Genere las matrices con el programa del Ejercicio 1: cada ejecucion\n"
             << "guarda una copia numerada (" << nombreArchivo(1) << ", "
             << nombreArchivo(2) << ", ...).\n";
        return;
    }

    const bool todoCoincide = all_of(tabla.begin(), tabla.end(),
                                     [](const FilaTabla& f) { return f.coinciden; });
    cout << "Matrices procesadas: " << tabla.size() << "\n";
    cout << (todoCoincide
                 ? "Verificacion: YYC, BT y el exhaustivo devolvieron exactamente el mismo "
                   "conjunto\nde testores tipicos en todas las matrices.\n"
                 : "ATENCION: hay matrices donde los conjuntos NO coinciden.\n");
    if (tabla.size() < 10)
        cout << "Aviso: el enunciado pide al menos diez matrices.\n";

    cout << "\nArchivos usados:\n";
    for (const FilaTabla& f : tabla) cout << "  " << f.nombre << " = " << f.archivo << "\n";
    cout << "\n";
    guardarCsv(tabla, "tabla_ej6.csv");
    guardarLatex(tabla, "tabla_ej6.tex");
}

void imprimirConjunto(const string& titulo, const set<set<int>>& testores) {
    cout << titulo << " (" << testores.size() << "):";
    if (testores.size() > 60) {
        cout << " (son demasiados para listarlos)\n";
        return;
    }
    for (const auto& t : testores) {
        cout << ' ';
        imprimirTestor(cout, t);
    }
    cout << "\n";
}

// Una sola matriz leida de un archivo, con los testores de cada algoritmo.
void ejecutarUnaMatriz() {
    cout << "Ruta del archivo (Enter para matriz_basica.txt): ";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string ruta;
    getline(cin, ruta);
    if (ruta.empty()) ruta = "matriz_basica.txt";

    Matriz M;
    if (!cargarMatriz(ruta, M)) return;

    cout << "\nMatriz basica (" << M.size() << " filas x " << M[0].size()
         << " columnas, densidad " << fixed << setprecision(4) << calcularDensidad(M)
         << "):\n";
    if (M.size() <= 30) imprimirMatriz(cout, M);
    else cout << "(se omite la impresion por espacio)\n";

    FilaTabla f = procesarMatriz(M, "matriz", ruta);

    cout << "\n";
    imprimirConjunto("Testores tipicos segun YYC       ", f.tipicosYYC);
    imprimirConjunto("Testores tipicos segun BT        ", f.tipicosBT);
    imprimirConjunto("Testores tipicos segun exhaustivo", f.tipicosExhaustivo);

    cout << "\nExhaustivo: reviso " << f.subconjuntos << " subconjuntos; " << f.testores
         << " son testores y " << f.tipicosExhaustivo.size() << " son tipicos.\n";
    cout << "BT: examino " << f.listasBT << " listas de " << f.subconjuntos
         << " posibles.\n\n";

    imprimirEncabezado();
    imprimirFila(f);
    cout << (f.coinciden ? "\nVerificacion: los tres algoritmos devolvieron exactamente "
                           "el mismo conjunto.\n"
                         : "\nATENCION: los conjuntos NO coinciden.\n");
}

int main() {
    cout << "====================================================================\n";
    cout << "   ALGORITMO EXHAUSTIVO Y COMPARACION CON YYC Y BT (EJ. 6)\n";
    cout << "====================================================================\n";

    while (true) {
        cout << "\n1. Experimento completo (archivos " << nombreArchivo(1) << ", "
             << nombreArchivo(2) << ", ...)\n";
        cout << "2. Procesar una sola matriz desde un archivo\n";
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
        if (opcion == 1) ejecutarExperimento();
        else if (opcion == 2) ejecutarUnaMatriz();
        else cout << "Opcion invalida.\n";
    }
    return 0;
}
