// Ejercicio 2.3. Búsqueda incremental de testores típicos con YYC.
// Referencia: Alba-Cabrera et al., sección 3, algoritmos 1 y 2, pp. 419–420.
//
// Un conjunto de columnas cubre una fila si alguna de esas columnas tiene un 1.
// Es testor si cubre todas las filas consideradas. Es típico si cada columna
// es indispensable para mantener esa cobertura.
//
// El Algoritmo 1 actualiza los testores típicos al incorporar cada fila.
// El Algoritmo 2 comprueba la tipicidad de los candidatos que se amplían.
// La lectura, la validación, el cronómetro, el CSV y el historial son parte del ejercicio.
// En C++ las filas y columnas empiezan en 0. Al mostrar los resultados,
// se numeran desde 1, como en el artículo.

#include <iostream>  // Entrada y salida de consola.
#include <vector>    // Matriz de filas y listas de columnas.
#include <set>       // Conjuntos ordenados y sin duplicados.
#include <algorithm> // Comprobaciones sobre conjuntos, filas y columnas.
#include <fstream>   // Lectura de matrices y escritura del CSV y del historial.
#include <limits>    // Límite usado para descartar el resto de una línea.
#include <sstream>   // Lectura de una línea y preparación del registro de la corrida.
#include <string>    // Texto de entrada y rutas de archivos.
#include <utility>   // std::move para transferir un vector o conjunto.
#include <chrono>    // Medición del tiempo de cálculo.
#include <ctime>     // Fecha y hora local del registro de la corrida.
#include <iomanip>   // Formato de los números mostrados.

using namespace std;

// ============================================================================
// FUNCIONES AUXILIARES PARA EL ALGORITMO YYC
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

// Da el mismo formato a cada testor en consola, CSV e historial.
void imprimirTestor(ostream& salida, const set<int>& testor) {
    salida << '{';
    for (auto it = testor.begin(); it != testor.end(); ++it) {
        if (it != testor.begin()) salida << ", ";
        // Por ejemplo, el índice 0 de C++ se muestra como x1.
        salida << 'x' << (*it + 1);
    }
    salida << '}';
}

// Escribe todos los testores con el separador elegido para cada salida.
void imprimirTestores(ostream& salida, const set<set<int>>& testores,
                      const char* separador) {
    for (auto it = testores.begin(); it != testores.end(); ++it) {
        if (it != testores.begin()) salida << separador;
        imprimirTestor(salida, *it);
    }
}

// Reporta el resultado al terminar una fila: cantidad, tiempo y testores de
// las filas vistas hasta ese momento. Cada fila de BM produce un registro CSV.
void mostrarResultado(ostream& csv, ostream& registro, int fila,
                      const set<set<int>>& testores, double tiempoFilaMs,
                      double tiempoAcumuladoMs) {
    cout << "--> Fila " << fila << " procesada | Tiempo acumulado: "
         << fixed << setprecision(6) << tiempoAcumuladoMs << " ms\n";
    cout << "    Testores típicos acumulados (" << testores.size() << "): ";
    imprimirTestores(cout, testores, " ");
    cout << "\n\n";

    // Se encierra la lista entre comillas porque los conjuntos contienen comas.
    csv << fila << ',' << testores.size() << ',' << fixed << setprecision(6)
        << tiempoAcumuladoMs << ",\"";
    imprimirTestores(csv, testores, "; ");
    csv << "\"\n";

    // El historial distingue el tiempo de esta fila del tiempo acumulado.
    registro << "| " << fila << " | " << testores.size() << " | "
             << fixed << setprecision(6) << tiempoFilaMs << " | "
             << tiempoAcumuladoMs << " | ";
    imprimirTestores(registro, testores, "; ");
    registro << " |\n";
}

// Inicia el registro Markdown con la fecha y una copia de la matriz procesada.
void iniciarRegistro(ostream& registro, const vector<vector<int>>& BM) {
    const time_t ahora = time(nullptr);
    const tm* fechaLocal = localtime(&ahora);
    registro << "\n## Corrida de YYC";
    if (fechaLocal) {
        registro << " | " << put_time(fechaLocal, "%Y-%m-%d %H:%M:%S");
    }
    registro << "\n\n### Matriz básica (" << BM.size() << " x "
             << BM.front().size() << ")\n\n```text\n";
    for (const auto& fila : BM) {
        for (size_t col = 0; col < fila.size(); ++col) {
            if (col != 0) registro << ' ';
            registro << fila[col];
        }
        registro << '\n';
    }
    registro << "```\n\n### Resultados por fila\n\n"
             << "| Fila | Cantidad | Tiempo de la fila (ms) | Tiempo acumulado (ms) | Testores típicos |\n"
             << "| ---: | ---: | ---: | ---: | --- |\n";
}

// Añade el registro completo al final del historial, conservando las corridas previas.
bool guardarHistorial(const string& nombreArchivo, const string& registro) {
    ofstream historial(nombreArchivo, ios::app);
    if (!historial) return false;
    historial << registro;
    historial.close();
    return static_cast<bool>(historial);
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

// ALGORITMO 1. Inicializa ψ* con r_1 y lo actualiza al avanzar por la matriz.
void ejecutarAlgoritmoYYC(const vector<vector<int>>& BM, ostream& csv,
                          ostream& registro) {
    // El reloj mide intervalos de cálculo y no depende de la hora del sistema.
    using Reloj = chrono::steady_clock;
    cout << "\n       EJECUCIÓN DE YYC FILA POR FILA\n\n";
    csv << "fila,cantidad,tiempo_acumulado_ms,testores\n";

    // Inicialización. Cada 1 de r_1 forma un testor típico de una columna.
    // Por ejemplo, en la Matriz A se inicia con {x1}, {x6} y {x7}.
    auto inicio = Reloj::now();
    set<set<int>> psiStar;
    for (int col = 0; col < static_cast<int>(BM[0].size()); ++col) {
        if (BM[0][col] == 1) psiStar.insert({col});
    }
    // La duración se suma antes de imprimir para medir el cálculo de YYC.
    double acumuladoMs = chrono::duration<double, milli>(Reloj::now() - inicio).count();
    mostrarResultado(csv, registro, 1, psiStar, acumuladoMs, acumuladoMs);

    // Desde r_2, cada fila reemplaza ψ* por los testores del nuevo prefijo.
    // Al mostrar la última fila, ψ* contiene los testores típicos de toda BM.
    for (int fila = 1; fila < static_cast<int>(BM.size()); ++fila) {
        inicio = Reloj::now();
        psiStar = actualizarTestores(BM, fila, psiStar);
        const double tiempoFilaMs = chrono::duration<double, milli>(Reloj::now() - inicio).count();
        acumuladoMs += tiempoFilaMs;
        mostrarResultado(csv, registro, fila + 1, psiStar, tiempoFilaMs, acumuladoMs);
    }

    // Después de la última fila, psiStar contiene los testores de toda la matriz.
    registro << "\n### Testores típicos finales (" << psiStar.size() << ")\n\n";
    imprimirTestores(registro, psiStar, "; ");
    registro << "\n\nTiempo total: " << fixed << setprecision(6) << acumuladoMs << " ms\n";
}

// LECTURA DE ARCHIVO. Primera línea: número de filas y número de columnas.
// Luego debe haber una línea por fila, con ceros y unos separados por espacios.
bool leerMatriz(istream& entrada, vector<vector<int>>& matriz) {
    string linea;
    if (!getline(entrada, linea)) return false;
    istringstream encabezado(linea);
    int filas, columnas;
    // Las dimensiones deben ser enteros positivos. ws salta espacios para
    // comprobar que el encabezado no contiene datos adicionales.
    if (!(encabezado >> filas >> columnas) || filas <= 0 || columnas <= 0 ||
        (encabezado >> ws).peek() != char_traits<char>::eof()) {
        return false;
    }

    // Comprueba que exista cada fila y que tenga exactamente los bits declarados.
    for (int fila = 0; fila < filas; ++fila) {
        if (!getline(entrada, linea)) return false;
        istringstream datos(linea);
        vector<int> valores;
        for (int col = 0; col < columnas; ++col) {
            string bit;
            if (!(datos >> bit) || (bit != "0" && bit != "1")) return false;
            // La comparación convierte el texto "1" en 1 y el texto "0" en 0.
            valores.push_back(bit == "1");
        }
        if ((datos >> ws).peek() != char_traits<char>::eof()) return false;
        matriz.push_back(std::move(valores));
    }
    // Al terminar solo se permiten espacios en blanco y el fin del archivo.
    return (entrada >> ws).eof() && !entrada.bad();
}

// VALIDACIÓN. YYC recibe una matriz básica ya construida. Las funciones de
// entrada garantizan que es rectangular y contiene solo ceros y unos.
bool esMatrizBasica(const vector<vector<int>>& matriz) {
    if (matriz.empty() || matriz.front().empty()) return false;

    // En binario, a <= b en cada posición significa que todos los unos de la
    // primera fila también están en la segunda. Así se detectan superfilas.
    const auto incluido = [](int a, int b) { return a <= b; };
    for (size_t i = 0; i < matriz.size(); ++i) {
        const auto& fila = matriz[i];
        // Una fila de ceros no se puede cubrir con ninguna columna.
        if (none_of(fila.begin(), fila.end(), [](int valor) { return valor == 1; })) {
            return false;
        }
        // Se compara cada par una vez, en ambos sentidos. Si las filas son
        // iguales, ambas comparaciones se cumplen y también se rechazan.
        for (size_t j = 0; j < i; ++j) {
            const auto& anterior = matriz[j];
            if (equal(fila.begin(), fila.end(), anterior.begin(), incluido) ||
                equal(anterior.begin(), anterior.end(), fila.begin(), incluido)) {
                return false;
            }
        }
    }
    return true;
}

// PROGRAMA PRINCIPAL. Selecciona la entrada, valida BM y guarda ambas salidas.
int main() {
    cout << "====================================================================\n";
    cout << "       IMPLEMENTACIÓN COMPUTACIONAL DEL ALGORITMO YYC (EJ. 2.3)\n";
    cout << "====================================================================\n\n";
    cout << "1. Usar la Matriz Básica A (7x7, D = 0.4082) del Ejercicio 2.1\n";
    cout << "3. Leer una Matriz Básica desde un archivo de texto\n";
    cout << "Opción: ";
    // Si la selección no se puede leer o está fuera del menú, se detiene.
    int opcion;
    if (!(cin >> opcion) || (opcion != 1 && opcion != 3)) {
        cerr << "Opción inválida. Elija 1 o 3.\n";
        return 1;
    }

    vector<vector<int>> BM;
    if (opcion == 1) {
        // Opción 1. Usa la Matriz Básica A del Ejercicio 2.1.
        BM = {
            {1, 0, 0, 0, 0, 1, 1},
            {1, 0, 1, 0, 0, 0, 0},
            {0, 0, 1, 0, 1, 1, 0},
            {0, 1, 0, 1, 0, 0, 1},
            {1, 1, 0, 0, 0, 0, 1},
            {0, 0, 1, 1, 0, 1, 0},
            {0, 1, 1, 0, 0, 0, 1}
        };
        cout << "\n-> Matriz Básica A cargada exitosamente (7 filas x 7 columnas).\n";
    } else {
        // Opción 3. Usa la ruta indicada o el nombre por defecto del menú.
        cout << "Ruta del archivo (Enter para matriz_basica.txt): ";
        // Descarta el salto de línea que dejó la lectura de la opción.
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

    // Ambas opciones pasan por la misma validación antes de ejecutar YYC.
    if (!esMatrizBasica(BM)) {
        cerr << "Se requiere una matriz básica no vacía, sin filas nulas, "
             << "duplicadas ni superfilas.\n";
        return 1;
    }

    // Abre el CSV y comprueba que se pueda escribir. Si ya existe con ese
    // nombre, la ejecución lo reemplaza con los resultados de la matriz actual.
    const string nombreCsv = "yyc_resultados.csv";
    ofstream csv(nombreCsv);
    if (!csv) {
        cerr << "No se pudo abrir " << nombreCsv << " para escritura.\n";
        return 1;
    }
    // Prepara la corrida completa en memoria. Las salidas se formatean fuera
    // de los intervalos cronometrados, igual que la impresión en consola.
    ostringstream registro;
    iniciarRegistro(registro, BM);
    ejecutarAlgoritmoYYC(BM, csv, registro);
    // Al cerrar se comprueba si la escritura se completó correctamente.
    csv.close();
    if (!csv) {
        cerr << "No se pudo completar la escritura de " << nombreCsv << ".\n";
        return 1;
    }
    cout << "Tabla guardada en " << nombreCsv << ".\n";

    // El CSV contiene la última corrida. El historial conserva todas las completadas.
    const string nombreHistorial = "historial_yyc.md";
    if (!guardarHistorial(nombreHistorial, registro.str())) {
        cerr << "No se pudo guardar la corrida en " << nombreHistorial << ".\n";
        return 1;
    }
    cout << "Corrida añadida a " << nombreHistorial << ".\n";
    return 0;
}
 
