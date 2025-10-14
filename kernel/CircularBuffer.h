// ================================================
//  @file      CircularBuffer.h
//  @author    Omar Mendiola
//  @brief     
//  @version   0.1
//  @date      2025-09-CURRENT_DAY
//
//  @copyright Copyright (c) 2025
// ================================================

#ifndef CIRCULARBUFFER_H_
#define CIRCULARBUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* =================== Includes =================== */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h> // Para memcpy

/* =================== Defines ==================== */
// #define EXAMPLE_DEFINE 1

/* =================== Types ====================== */
/**
 * @brief Estructura de control para el buffer circular.
 * @warning ¡No accedas a sus miembros directamente! Usa la API proporcionada.
 */
typedef struct {
    uint8_t* buffer;            ///< Puntero al array de datos subyacente.
    volatile size_t head;       ///< Índice donde se escribirá el próximo elemento. Modificado por el productor.
    volatile size_t tail;       ///< Índice desde donde se leerá el próximo elemento. Modificado por el consumidor.
    size_t max_elements;        ///< Tamaño total del array de datos (capacidad útil + 1).
    size_t element_size;        ///< Tamaño en bytes de un solo elemento en el buffer.
} cbuf_handle_t;

/* =================== Constants ================== */
// static const int EXAMPLE_CONST = 42;

/* =================== Macros ===================== */
/**
 * @brief Macro para entrar en una sección crítica.
 * @details El usuario DEBE definir esto para su plataforma específica si usa el buffer
 * entre un contexto de interrupción (ISR) y el bucle principal. Esto garantiza
 * que las operaciones sobre los índices del buffer sean atómicas.
 * @note Ejemplo para ARM Cortex-M: #define CIRCULAR_BUF_ENTER_CRITICAL() __disable_irq()
 * @note Ejemplo para AVR:
 * #include <util/atomic.h>
 * #define CIRCULAR_BUF_ENTER_CRITICAL() ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
 */
#ifndef CIRCULAR_BUF_ENTER_CRITICAL
#define CIRCULAR_BUF_ENTER_CRITICAL()   // Por defecto, no hace nada
#endif

/**
 * @brief Macro para salir de una sección crítica.
 * @details Complemento de CIRCULAR_BUF_ENTER_CRITICAL.
 * @note Ejemplo para ARM Cortex-M: #define CIRCULAR_BUF_EXIT_CRITICAL() __enable_irq()
 * @note Ejemplo para AVR: #define CIRCULAR_BUF_EXIT_CRITICAL() }
 */
#ifndef CIRCULAR_BUF_EXIT_CRITICAL
#define CIRCULAR_BUF_EXIT_CRITICAL()    // Por defecto, no hace nada
#endif

/**
 * @brief MACRO para declarar y definir estáticamente un buffer circular.
 *
 * @details Esta macro crea la variable del handle y el array de datos subyacente en tiempo de compilación.
 * Elimina la necesidad de una función de inicialización explícita.
 *
 * @param _name El nombre de la variable para el handle del buffer (ej. my_buffer).
 * @param _type El tipo de dato que almacenará el buffer (ej. uint8_t, can_message_t).
 * @param _capacity La cantidad máxima de elementos que el buffer podrá contener (capacidad útil).
 */
#define CIRCULAR_BUF_DEFINE(_name, _type, _capacity)                \
    static _type _name##_data_buffer[(_capacity) + 1];              \
    cbuf_handle_t _name = {                                         \
        .buffer = (uint8_t*)_name##_data_buffer,                    \
        .head = 0,                                                  \
        .tail = 0,                                                  \
        .max_elements = (_capacity) + 1,                            \
        .element_size = sizeof(_type)                               \
    }

/* =================== Variables ================== */

/* =================== Functions ================== */
/**
 * @fn      static inline void circular_buf_reset(cbuf_handle_t* cbuf)
 * @brief   Resetea el buffer a un estado vacío.
 * @details Descarta todos los elementos del buffer moviendo el puntero de cola (`tail`)
 * a la posición de la cabeza (`head`). Es una operación segura para concurrencia.
 * @param[in,out] cbuf  Puntero al handle del buffer que se va a resetear.
 */
static inline void circular_buf_reset(cbuf_handle_t* cbuf) {
    CIRCULAR_BUF_ENTER_CRITICAL();
    cbuf->tail = cbuf->head;
    CIRCULAR_BUF_EXIT_CRITICAL();
}

/**
 * @fn      static inline bool circular_buf_is_full(cbuf_handle_t* cbuf)
 * @brief   Comprueba si el buffer está lleno.
 * @param[in] cbuf  Puntero al handle del buffer.
 * @return          true si el buffer ha alcanzado su capacidad máxima, false en caso contrario.
 */
static inline bool circular_buf_is_full(cbuf_handle_t* cbuf) {
    return ((cbuf->head + 1) % cbuf->max_elements) == cbuf->tail;
}

/**
 * @fn      static inline bool circular_buf_is_empty(cbuf_handle_t* cbuf)
 * @brief   Comprueba si el buffer está vacío.
 * @param[in] cbuf  Puntero al handle del buffer.
 * @return          true si el buffer no contiene elementos, false en caso contrario.
 */
static inline bool circular_buf_is_empty(cbuf_handle_t* cbuf) {
    return cbuf->head == cbuf->tail;
}

/**
 * @fn      static inline size_t circular_buf_get_capacity(cbuf_handle_t* cbuf)
 * @brief   Retorna la capacidad máxima útil del buffer.
 * @param[in] cbuf  Puntero al handle del buffer.
 * @return          El número máximo de elementos que el buffer puede almacenar.
 */
static inline size_t circular_buf_get_capacity(cbuf_handle_t* cbuf) {
    return cbuf->max_elements - 1;
}

/**
 * @fn      static inline size_t circular_buf_get_size(cbuf_handle_t* cbuf)
 * @brief   Retorna el número actual de elementos en el buffer.
 * @details Esta función es segura para ser llamada en un entorno concurrente.
 * @param[in] cbuf  Puntero al handle del buffer.
 * @return          El número de elementos actualmente almacenados en el buffer.
 */
static inline size_t circular_buf_get_size(cbuf_handle_t* cbuf) {
    size_t size = 0;
    CIRCULAR_BUF_ENTER_CRITICAL();
    if (cbuf->head >= cbuf->tail) {
        size = cbuf->head - cbuf->tail;
    } else {
        size = cbuf->max_elements + cbuf->head - cbuf->tail;
    }
    CIRCULAR_BUF_EXIT_CRITICAL();
    return size;
}

/**
 * @fn      static inline bool circular_buf_push(cbuf_handle_t* cbuf, const void* data)
 * @brief   Añade un elemento al final del buffer (escritura).
 * @details Esta operación es atómica y segura para ser llamada desde una ISR (productor).
 * @param[in,out] cbuf  Puntero al handle del buffer.
 * @param[in]     data  Puntero al elemento que se va a añadir. El contenido será copiado al buffer.
 * @return              true si el elemento se añadió con éxito, false si el buffer estaba lleno (overflow).
 */
static inline bool circular_buf_push(cbuf_handle_t* cbuf, const void* data) {
    size_t next_head = (cbuf->head + 1) % cbuf->max_elements;
    size_t current_tail;
    CIRCULAR_BUF_ENTER_CRITICAL();
    current_tail = cbuf->tail;
    CIRCULAR_BUF_EXIT_CRITICAL();
    if (next_head == current_tail) {
        return false;
    }
    uint8_t* dest = cbuf->buffer + (cbuf->head * cbuf->element_size);
    memcpy(dest, data, cbuf->element_size);
    CIRCULAR_BUF_ENTER_CRITICAL();
    cbuf->head = next_head;
    CIRCULAR_BUF_EXIT_CRITICAL();
    return true;
}

/**
 * @fn      static inline bool circular_buf_pop(cbuf_handle_t* cbuf, void* data)
 * @brief   Extrae el primer elemento del buffer (lectura).
 * @details Esta operación es atómica y segura para ser llamada desde el bucle principal (consumidor).
 * @param[in,out] cbuf  Puntero al handle del buffer.
 * @param[out]    data  Puntero a una variable donde se guardará el elemento extraído.
 * @return              true si un elemento fue extraído con éxito, false si el buffer estaba vacío (underflow).
 */
static inline bool circular_buf_pop(cbuf_handle_t* cbuf, void* data) {
    size_t current_head;
    CIRCULAR_BUF_ENTER_CRITICAL();
    current_head = cbuf->head;
    CIRCULAR_BUF_EXIT_CRITICAL();
    if (current_head == cbuf->tail) {
        return false;
    }
    uint8_t* src = cbuf->buffer + (cbuf->tail * cbuf->element_size);
    memcpy(data, src, cbuf->element_size);
    CIRCULAR_BUF_ENTER_CRITICAL();
    cbuf->tail = (cbuf->tail + 1) % cbuf->max_elements;
    CIRCULAR_BUF_EXIT_CRITICAL();
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // CIRCULARBUFFER_H_