//
//  Ejercicio2.cpp
//  MyTool
//
//  Created by Nicole Rafaela Fuentes Manosalvas on 10/31/26.
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
#include <chrono>   // Librería para medir tiempos de ejecución con alta precisión
#include <iomanip>  // Para dar formato limpio a la salida de consola

using namespace std;

// ============================================================================
// FUNCIONES AUXILIARES PARA EL ALGORITMO YYC
// ============================================================================

// 1. Función que verifica si un subconjunto de columnas es un TESTOR en las filas leídas (0 a filaActual)
bool esTestor(const set<int>& candidatos, const vector<vector<int>>& BM, int filaActual) {
    // Recorre todas las filas leídas hasta el momento
    for (int i = 0; i <= filaActual; ++i) {
        bool tieneUno = false;
        // Revisa si en la fila 'i' hay al menos un 1 en las columnas del candidato
        for (int col : candidatos) {
            if (BM[i][col] == 1) {
                tieneUno = true;
                break; // Esta fila sí tiene un 1, pasamos a revisar la siguiente fila
            }
        }
        // Si una fila completa quedó en ceros, NO es testor
        if (!tieneUno) {
            return false;
        }
    }
    return true; // No hubo filas de ceros, SÍ es testor
}

// 2. Función que verifica si un testor es TÍPICO (irreductible)
// Un testor es típico si al quitarle CUALQUIER variable, deja de ser testor.
bool esTipico(const set<int>& candidatos, const vector<vector<int>>& BM, int filaActual) {
    // Precondición: candidatos ya cubre las filas de F1 hasta filaActual.
    // Probamos quitar cada variable 'v' una por una
    for (int v : candidatos) {
        set<int> subconjunto = candidatos;
        subconjunto.erase(v); // Eliminamos temporalmente la variable 'v'
        
        // Si al quitar 'v' el resto SIGUE siendo testor, entonces 'v' sobraba -> NO es típico
        if (esTestor(subconjunto, BM, filaActual)) {
            return false;
        }
    }
    return true; // Cada variable era indispensable -> SÍ es típico
}

// Las dos salidas usan la misma representación de los conjuntos.
void imprimirTestor(ostream& salida, const set<int>& testor) {
    salida << '{';
    for (auto it = testor.begin(); it != testor.end(); ++it) {
        if (it != testor.begin()) salida << ", ";
        salida << 'x' << (*it + 1);
    }
    salida << '}';
}

void mostrarResultado(ostream& csv, int fila, const set<set<int>>& testores, double tiempoMs) {
    cout << "--> Fila " << fila << " procesada | Tiempo acumulado: "
         << fixed << setprecision(6) << tiempoMs << " ms\n";
    cout << "    Testores típicos acumulados (" << testores.size() << "): ";
    csv << fila << ',' << testores.size() << ',' << fixed << setprecision(6)
        << tiempoMs << ",\"";
    for (auto it = testores.begin(); it != testores.end(); ++it) {
        if (it != testores.begin()) {
            cout << ' ';
            csv << "; ";
        }
        imprimirTestor(cout, *it);
        imprimirTestor(csv, *it);
    }
    cout << "\n\n";
    csv << "\"\n";
}

set<set<int>> actualizarTestores(const vector<vector<int>>& BM, int fila,
                               const set<set<int>>& testoresPrevios) {
    set<set<int>> testoresActuales;
    vector<int> columnasConUno;
    for (int col = 0; col < static_cast<int>(BM[fila].size()); ++col) {
        if (BM[fila][col] == 1) columnasConUno.push_back(col);
    }

    for (const auto& testor : testoresPrevios) {
        const bool cubreFila = any_of(testor.begin(), testor.end(),
                                     [&](int col) { return BM[fila][col] == 1; });
        if (cubreFila) {
            testoresActuales.insert(testor);
            continue;
        }

        // El candidato conserva la cobertura anterior y cubre la fila nueva.
        for (int col : columnasConUno) {
            set<int> candidato = testor;
            candidato.insert(col);
            if (esTipico(candidato, BM, fila)) {
                testoresActuales.insert(candidato);
            }
        }
    }
    return testoresActuales;
}

void ejecutarAlgoritmoYYC(const vector<vector<int>>& BM, ostream& csv) {
    using Reloj = chrono::steady_clock;
    cout << "\n       EJECUCIÓN DE YYC FILA POR FILA\n\n";
    csv << "fila,cantidad,tiempo_acumulado_ms,testores\n";

    auto inicio = Reloj::now();
    set<set<int>> psiStar;
    for (int col = 0; col < static_cast<int>(BM[0].size()); ++col) {
        if (BM[0][col] == 1) psiStar.insert({col});
    }
    double acumuladoMs = chrono::duration<double, milli>(Reloj::now() - inicio).count();
    mostrarResultado(csv, 1, psiStar, acumuladoMs);

    for (int fila = 1; fila < static_cast<int>(BM.size()); ++fila) {
        inicio = Reloj::now();
        psiStar = actualizarTestores(BM, fila, psiStar);
        acumuladoMs += chrono::duration<double, milli>(Reloj::now() - inicio).count();
        mostrarResultado(csv, fila + 1, psiStar, acumuladoMs);
    }
}

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
        if (none_of(fila.begin(), fila.end(), [](int valor) { return valor == 1; })) {
            return false;
        }
        // Una inclusión en cualquiera de los sentidos incluye también la igualdad.
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

int main() {
    cout << "====================================================================\n";
    cout << "       IMPLEMENTACIÓN COMPUTACIONAL DEL ALGORITMO YYC (EJ. 2.3)\n";
    cout << "====================================================================\n\n";
    cout << "1. Usar la Matriz Básica A (7x7, D = 0.4082) del Ejercicio 2.1\n";
    cout << "2. Ingresar una Matriz Básica personalizada\n";
    cout << "3. Leer una Matriz Básica desde un archivo de texto\n";
    cout << "Opción: ";
    int opcion;
    if (!(cin >> opcion) || opcion < 1 || opcion > 3) {
        cerr << "Opción inválida. Elija 1, 2 o 3.\n";
        return 1;
    }

    vector<vector<int>> BM;
    if (opcion == 1) {
        // Matriz Básica A generada en el Ejercicio 2.1
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
    } else if (opcion == 2) {
        int filas, columnas;
        cout << "Ingrese el número de filas: ";
        if (!(cin >> filas) || filas <= 0) {
            cerr << "El número de filas debe ser un entero positivo.\n";
            return 1;
        }
        cout << "Ingrese el número de columnas: ";
        if (!(cin >> columnas) || columnas <= 0) {
            cerr << "El número de columnas debe ser un entero positivo.\n";
            return 1;
        }
        cout << "Ingrese los elementos de la matriz (0 y 1):\n";
        for (int fila = 0; fila < filas; ++fila) {
            vector<int> valores;
            for (int col = 0; col < columnas; ++col) {
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
        cerr << "Se requiere una matriz básica no vacía, sin filas nulas, "
             << "duplicadas ni superfilas.\n";
        return 1;
    }

    const string nombreCsv = "yyc_resultados.csv";
    ofstream csv(nombreCsv);
    if (!csv) {
        cerr << "No se pudo abrir " << nombreCsv << " para escritura.\n";
        return 1;
    }
    ejecutarAlgoritmoYYC(BM, csv);
    csv.close();
    if (!csv) {
        cerr << "No se pudo completar la escritura de " << nombreCsv << ".\n";
        return 1;
    }
    cout << "Tabla guardada en " << nombreCsv << ".\n";
    return 0;
}
