#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор входных файлов для задачи N-тел (nbody.c).

Формат входного файла:
    n
    масса x y z vx vy vz
    ...

Использование:
    python3 gen_input.py <preset> <outfile> [size]
где size (необязательно) — число тел для пресетов binary / cluster
(по умолчанию: binary=2, cluster=8).

Пресеты:
    binary   — две равные массы, круговая орбита вокруг общего ЦМ (проверка ИМПУЛЬСА)
    triplet  — три равные массы в равностороннем треугольнике (проверка ЦМ/симметрии)
    cluster  — случайный набор тел (общий тест)
    single   — одна частица без сил (проверка инерции: движется прямолинейно)
"""
import math
import random
import sys

G = 6.67430e-11


def write(out, bodies):
    with open(out, "w") as f:
        f.write(f"{len(bodies)}\n")
        for (mass, x, y, z, vx, vy, vz) in bodies:
            f.write(f"{mass:.10g} {x:.10g} {y:.10g} {z:.10g} "
                    f"{vx:.10g} {vy:.10g} {vz:.10g}\n")


def binary(out, mass=1e11, radius=1.0, num=2):
    """num тел по кругу радиуса radius, все на орбите вокруг общего ЦМ."""
    bodies = []
    for i in range(num):
        ang = 2 * math.pi * i / num
        x, y = radius * math.cos(ang), radius * math.sin(ang)
        # скорость для круговой орбиты в поле симметричного кольца тел:
        # v = sqrt(G * (сумма остальных масс) / (4 * r))  для пары равных масс
        others = mass * (num - 1)
        v = math.sqrt(G * others / (4.0 * radius))
        vx, vy = -v * math.sin(ang), v * math.cos(ang)
        bodies.append((mass, x, y, 0.0, vx, vy, 0.0))
    write(out, bodies)


def triplet(out, mass=1e11, radius=1.0):
    """Три равные массы в равностороннем треугольнике с нулевым импульсом.
    ЦМ в нуле, суммарный импульс = 0: под действием гравитации тела падают
    к центру, при этом ЦМ остаётся неподвижным
    """
    bodies = []
    for i in range(3):
        ang = math.pi / 6 + 2 * math.pi * i / 3
        x, y = radius * math.cos(ang), radius * math.sin(ang)
        bodies.append((mass, x, y, 0.0, 0.0, 0.0, 0.0))
    write(out, bodies)


def cluster(out, num=8, mass=1e9, spread=3.0, seed=1):
    """Случайный набор тел со случайными скоростями."""
    rnd = random.Random(seed)
    bodies = []
    for _ in range(num):
        m = mass * (0.5 + rnd.random())
        x = spread * (rnd.random() - 0.5)
        y = spread * (rnd.random() - 0.5)
        z = spread * (rnd.random() - 0.5)
        vx = 0.2 * (rnd.random() - 0.5)
        vy = 0.2 * (rnd.random() - 0.5)
        vz = 0.2 * (rnd.random() - 0.5)
        bodies.append((m, x, y, z, vx, vy, vz))
    write(out, bodies)


def single(out, mass=1e9):
    """Одна частица без соседей: сила = 0, движение по инерции."""
    write(out, [(mass, 0.0, 0.0, 0.0, 1.0, 0.5, 0.0)])


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    preset = sys.argv[1]
    out = sys.argv[2]
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    if preset == "binary":
        binary(out, num=size if size else 2)
    elif preset == "triplet":
        triplet(out)
    elif preset == "cluster":
        cluster(out, num=size if size else 8)
    elif preset == "single":
        single(out)
    else:
        print("Unknown preset:", preset)
        sys.exit(1)
    print(f"OK: preset='{preset}' -> {out}")


if __name__ == "__main__":
    main()