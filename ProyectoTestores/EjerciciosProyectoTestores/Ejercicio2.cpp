//
//  Ejercicio2.cpp
//  MyTool
//
//  Created by Nicole Rafaela Fuentes Manosalvas on 10/31/26.
//

#include <iostream>
#include <vector>
#include <set>
#include <algorithm> // Para usar includes (verificar si un conjunto contiene a otro)
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
    // Si el candidato solo tiene 1 elemento, automáticamente es típico
    if (candidatos.size() <= 1) return true;

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

// 3. Función auxiliar para imprimir un testor en formato matemático (ej. {x1, x3})
void imprimirTestor(const set<int>& t) {
    cout << "{";
    auto it = t.begin();
    while (it != t.end()) {
        cout << "x" << (*it + 1); // Sumamos 1 para mostrar x1 en lugar de x0
        ++it;
        if (it != t.end()) cout << ", ";
    }
    cout << "}";
}

// ALGORITMO YYC

void ejecutarAlgoritmoYYC(const vector<vector<int>>& BM) {
    int numFilas = (int)BM.size();
    int numCols = (int)BM[0].size();

    cout << "\n====================================================================";
    cout << "\n       EJECUCIÓN DEL ALGORITMO YYC (PROCESAMIENTO FILA POR FILA)    ";
    cout << "\n====================================================================\n\n";

    // Medición del tiempo inicial global
    auto tiempoInicioGlobal = chrono::high_resolution_clock::now();

    // Conjunto que almacenará los testores típicos acumulados (Psi*)
    set<set<int>> psiStar;

    // ------------------------------------------------------------------------
    // PASO 1: PROCESAR LA PRIMERA FILA (Fila 0 / F1)
    // ------------------------------------------------------------------------
    // Para la primera fila, cada columna que tenga un 1 es un testor típico inicial
    for (int j = 0; j < numCols; ++j) {
        if (BM[0][j] == 1) {
            psiStar.insert({j}); // Guarda la variable individual {xj}
        }
    }

    // Registra el tiempo al terminar la primera fila
    auto tiempoActual = chrono::high_resolution_clock::now();
    double tiempoAcumuladoMs = chrono::duration<double, milli>(tiempoActual - tiempoInicioGlobal).count();

    // Muestra los resultados de la Fila 1
    cout << "--> Fila 1 procesada | Tiempo acumulado: " << fixed << setprecision(4) << tiempoAcumuladoMs << " ms\n";
    cout << "    Testores típicos acumulados (" << psiStar.size() << "): ";
    for (const auto& t : psiStar) { imprimirTestor(t); cout << " "; }
    cout << "\n\n";

    // PASO 2: PROCESAR LAS FILAS RESTANTES (Fila 1 a numFilas - 1)

    for (int i = 1; i < numFilas; ++i) {
        set<set<int>> auxPsi; // Almacenará la lista actualizada de testores para la iteración 'i'

        // Identificamos qué columnas tienen un 1 en la nueva fila 'i'
        vector<int> colsConUno;
        for (int j = 0; j < numCols; ++j) {
            if (BM[i][j] == 1) {
                colsConUno.push_back(j);
            }
        }

        // Evaluamos cada testor previo guardado en psiStar
        for (const auto& tau : psiStar) {
            
            // Caso A: ¿El testor actual ya tiene un 1 en la nueva fila 'i'?
            bool preservado = false;
            for (int col : tau) {
                if (BM[i][col] == 1) {
                    preservado = true;
                    break;
                }
            }

            if (preservado) {
                // Se conserva tal como está porque sigue siendo testor
                auxPsi.insert(tau);
            } else {
                // Caso B: El testor falló en la nueva fila (quedó en ceros).
                // Lo extendemos combinándolo con cada columna que tenga un 1 en la nueva fila
                for (int colExt : colsConUno) {
                    set<int> nuevoCandidato = tau;
                    nuevoCandidato.insert(colExt); // tau U {colExt}

                    // Verificamos si la nueva combinación es TÍPICA
                    if (esTipico(nuevoCandidato, BM, i)) {
                        auxPsi.insert(nuevoCandidato);
                    }
                }
            }
        }

        // Limpieza final de redundancias en auxPsi (eliminar superconjuntos)
        set<set<int>> psiLimpio;
        for (const auto& t1 : auxPsi) {
            bool esRedundante = false;
            for (const auto& t2 : auxPsi) {
                if (t1 != t2 && includes(t1.begin(), t1.end(), t2.begin(), t2.end())) {
                    esRedundante = true; // t1 contiene a t2, por ende t1 no es mínimo/típico
                    break;
                }
            }
            if (!esRedundante) {
                psiLimpio.insert(t1);
            }
        }

        // Actualizamos psiStar con el conjunto limpio
        psiStar = psiLimpio;

        // Medimos el tiempo de ejecución acumulado tras procesar la fila 'i'
        tiempoActual = chrono::high_resolution_clock::now();
        tiempoAcumuladoMs = chrono::duration<double, milli>(tiempoActual - tiempoInicioGlobal).count();

        // Desplegamos resultados requeridos
        cout << "--> Fila " << (i + 1) << " procesada | Tiempo acumulado: " << fixed << setprecision(4) << tiempoAcumuladoMs << " ms\n";
        cout << "    Testores típicos acumulados (" << psiStar.size() << "): ";
        for (const auto& t : psiStar) { imprimirTestor(t); cout << " "; }
        cout << "\n\n";
    }
}

// FUNCIÓN PRINCIPAL / INTERFAZ DE USUARIO

int main() {
    int opcion;

    cout << "====================================================================\n";
    cout << "       IMPLEMENTACIÓN COMPUTACIONAL DEL ALGORITMO YYC (EJ. 2.3)    \n";
    cout << "====================================================================\n\n";
    cout << "Seleccione una opción de entrada:\n";
    cout << "1. Usar la Matriz Básica A (7x7, D = 0.4082) del Ejercicio 2.1\n";
    cout << "2. Ingresar una Matriz Básica personalizada\n";
    cout << "Opción: ";
    cin >> opcion;

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
    } else {
        int filas, cols;
        cout << "Ingrese el número de filas: "; cin >> filas;
        cout << "Ingrese el número de columnas: "; cin >> cols;
        BM.resize(filas, vector<int>(cols));
        cout << "Ingrese los elementos de la matriz (0s y 1s):\n";
        for (int i = 0; i < filas; ++i) {
            for (int j = 0; j < cols; ++j) {
                cin >> BM[i][j];
            }
        }
    }

    // Ejecuta el algoritmo YYC midiendo tiempos y mostrando resultados fila por fila
    ejecutarAlgoritmoYYC(BM);

    return 0;
}
