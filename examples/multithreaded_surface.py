import pygame
import threading
import random

pygame.init()

surface = pygame.Surface((7, 1))
surface.fill("black")

LERP_OFFSET = 0.0001

# def get_random_color() -> pygame.Color:
#     r = random.randint(0, 255)
#     g = random.randint(0, 255)
#     b = random.randint(0, 255)
#     a = 255
#     return pygame.Color(r, g, b, a)


def multithreaded_func(
    surf: pygame.Surface, target_pixel: tuple[int, int], target_color: pygame.Color
) -> None:
    lerp_distance = 0

    original_color = surf.get_at(target_pixel)

    while surf.get_at(target_pixel) != target_color:
        lerp_distance += LERP_OFFSET
        new_color = original_color.lerp(target_color, lerp_distance)
        surf.set_at(target_pixel, new_color)


pixels = [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (6, 0)]

# colors = [get_random_color() for _ in range(surface.width)]
colors = [
    pygame.Color(255, 0, 0),
    pygame.Color(255, 127, 0),
    pygame.Color(255, 255, 0),
    pygame.Color(0, 255, 0),
    pygame.Color(0, 0, 255),
    pygame.Color(75, 0, 130),
    pygame.Color(148, 0, 211),
]

threads = []
for pixel, color in zip(pixels, colors):
    new_thread = threading.Thread(
        target=multithreaded_func, args=(surface, pixel, color)
    )
    new_thread.start()
    threads.append(new_thread)

for thread in threads:
    thread.join()

# for pixel, color in zip(pixels, colors):
#     surface.set_at(pixel, color)

pygame.image.save(pygame.transform.scale_by(surface, 10), "out.png")
