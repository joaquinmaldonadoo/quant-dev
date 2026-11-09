# quant-dev

Backtester de un market maker en C++.

La estrategia es Avellaneda-Stoikov. Pone una orden de compra y una de venta alrededor del precio y las va corriendo según el inventario que tiene. Tiene una señal con el microprice y el order flow imbalance para que se adelante un poco a dónde va el precio.


## Qué tiene

- Order book L2
- Features: microprice, imbalance, OFI y volatilidad
- La estrategia y un risk manager con límite de inventario
- Un simulador de exchange con latencia y cola de órdenes
- Métricas de PnL, inventario y markouts
- Tests con GoogleTest

## Cómo correrlo

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

Hay un generador de datos sintéticos:

```bash
PYTHONPATH=python python3 -m hft_tools.synthetic --out data/sintetico.csv --events 500000
./build/hft_backtest --config configs/base.ini --data data/sintetico.csv --out results/prueba
```

Los resultados quedan en `results/prueba`.


## Falta:

- Calibrar los parámetros
