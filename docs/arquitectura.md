# Arquitectura

El sistema es un backtester event-driven para un market maker. Todo lo que corre por evento de mercado está en C++. Lo que no es camino caliente (generar datos, calibrar, analizar) está en Python. Los dos se hablan con archivos CSV, así no hay que mantener bindings.

## Flujo de un evento

```
CSV de eventos ──> CsvFeed (thread lector) ──SPSC──> BacktestEngine
                                                        │
                       ┌────────────────────────────────┤
                       ▼                                ▼
                   OrderBook ──> Features ──> AvellanedaStoikov ──> RiskManager ──> OrderManager
                       │                                                               │
                       ▼                                                               │ (latencia)
                  ExchangeSim <────────────────────────────────────────────────────────┘
                       │ fills (latencia)
                       ▼
               OrderManager ──> Portfolio ──> Metrics ──> CSV de resultados
```

1. El feed lee el CSV en otro thread y pasa los eventos por una cola SPSC.
2. El engine aplica el evento al order book.
3. El simulador de exchange revisa si ese evento ejecuta alguna de nuestras órdenes (cola, trades, cruces).
4. Las features se actualizan (microprice, imbalance, OFI, volatilidad).
5. La estrategia calcula las cotizaciones que quiere tener.
6. El risk manager las recorta o las bloquea.
7. El OMS compara lo que quiere con lo que tiene vivo y manda altas y cancelaciones.
8. Las órdenes y cancelaciones llegan al exchange simulado después de `latency_ns`. Los fills vuelven a la estrategia con la misma latencia.

## Módulos

| Módulo | Archivo | Qué hace |
|---|---|---|
| Tipos | `types.hpp` | Precio en ticks, cantidad en lotes, timestamps en ns. Todo entero |
| Config | `config.*` | Lee el `.ini`, valida rangos y rechaza claves desconocidas |
| Feed | `csv_feed.*`, `spsc_queue.hpp` | Parseo del CSV en un thread aparte y cola lock-free hacia el engine |
| Order book | `order_book.*` | Book L2 con arrays indexados por precio |
| Features | `features.*` | Microprice, imbalance multinivel, OFI con ventana, volatilidad EWMA |
| Estrategia | `avellaneda_stoikov.*` | Precio de reserva, spread óptimo y sesgo por la señal |
| Riesgo | `risk_manager.*` | Límite de inventario, tamaño máximo, tasa de órdenes y kill switch por drawdown |
| OMS | `order_manager.*` | Estado de cada orden y diferencia entre cotización deseada y viva |
| Simulador | `exchange_sim.*` | Llegada con latencia, posición en la cola, fills |
| Portfolio | `portfolio.*` | Inventario, caja, fees y PnL marcado a mid |
| Métricas | `metrics.*` | Series de PnL, markouts, drawdown y resumen |
| Engine | `backtest_engine.*` | Junta todo y ordena eventos de mercado y eventos internos por tiempo |
| Logger | `logger.*` | Logs con nivel, fuera del camino caliente |

## Decisiones

**Precios enteros.** El book y el matching trabajan en ticks. Comparar doubles para saber si dos órdenes están en el mismo nivel es una fuente de bugs clásica.

**Book con arrays densos.** Cada lado es un `std::vector<Qty>` indexado por `precio - min_price`. Actualizar un nivel es O(1). Buscar el nuevo mejor precio cuando se vacía el mejor nivel recorre pocos ticks porque los niveles cercanos casi siempre tienen volumen. El costo es memoria: hay que configurar el rango de precios. Para BTC con tick de 0.1 y un rango de 30k a 130k son 1M de niveles por lado, unos 8 MB, que entra sin problema. Un `std::map` sería más flexible pero cada acceso es O(log n) con saltos de puntero.

**Un solo thread para la lógica.** El backtest tiene que ser determinístico. Estrategia, simulador y portfolio corren en el mismo thread. El único paralelismo es el parseo del CSV, que se solapa con el procesamiento usando una cola SPSC. Como hay un solo productor y un solo consumidor, el orden de los eventos se mantiene.

**Sin asignaciones en el camino caliente.** Las órdenes vivas son pocas (una o dos por lado), así que el OMS usa un array chico. La cola de eventos internos reserva memoria al arrancar.

**Python y C++ por archivos.** El motor lee un CSV y escribe CSVs de resultados. Python genera datos, calibra parámetros y analiza. Es más simple de testear y de reproducir que usar pybind11.
