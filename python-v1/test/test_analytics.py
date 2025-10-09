import graphblas as gb
import analytics as analytics

test_grb_1 = gb.Matrix.from_coo([0, 0, 0, 0, 2, 2, 2, 3, 3, 3, 4, 4],
                                [1, 2, 3, 4, 1, 2, 4, 2, 3, 4, 2, 4],
                                [1, 1, 2, 1, 4, 2, 1, 2, 4, 1, 1, 1],
                                dtype=int,
                                nrows=2**32,
                                ncols=2**32
                                )

test_grb_2 = gb.Matrix.from_coo([0, 3], [1, 2], [1, 1], dtype=int, nrows=5, ncols=5)

old_IP_limit = 10


def test_valid_packets():
    n_valid_packets = analytics.valid_packets(test_grb_1)
    assert n_valid_packets == 21


def test_valid_packets_return_max_packets():
    _, max_packets = analytics.valid_packets(test_grb_1, return_max_packets=True)
    assert max_packets == 4


def test_valid_packets_return_IP_pairs():
    _, max_IP_pairs = analytics.valid_packets(test_grb_1, return_max_IP_pairs=True)
    assert max_IP_pairs == 2  # [(2, 1), (3, 3)]


def test_valid_packets_return_max_and_IP_pairs():
    _, max_packets, max_IP_pairs = analytics.valid_packets(test_grb_1,
                                                           return_max_packets=True,
                                                           return_max_IP_pairs=True
                                                           )
    assert max_packets == 4
    assert max_IP_pairs == 2  # [(2, 1), (3, 3)]


def test_max_packets_by_sources():
    max_packet_count = analytics.max_packets_by_sources(test_grb_1)
    assert max_packet_count == 7


def test_max_packets_by_sources_return_sources():
    _, max_sources = analytics.max_packets_by_sources(test_grb_1, return_max_sources=True)
    assert max_sources == 2  # [2, 3]


def test_max_packets_by_destinations():
    max_packet_count = analytics.max_packets_by_destinations(test_grb_1)
    assert max_packet_count == 6


def test_max_packets_by_destinations_return_destinations():
    _, max_destinations = analytics.max_packets_by_destinations(test_grb_1, return_max_destinations=True)
    assert max_destinations == 2  # [2, 3]


def test_unique_links():
    n_unique_links = analytics.unique_links(test_grb_1)
    assert n_unique_links == 12


def test_unique_sources():
    n_unique_sources = analytics.unique_sources(test_grb_1)
    assert n_unique_sources == 4


def test_unique_sources_return_max_fan_out():
    _, max_fan_out = analytics.unique_sources(test_grb_1, return_max_fan_out=True)
    assert max_fan_out == 4


def test_unique_sources_return_max_sources():
    _, max_sources = analytics.unique_sources(test_grb_1, return_max_sources=True)
    assert max_sources == 1  # [0]


def test_unique_sources_return_max_fan_out_and_sources():
    _, max_fan_out, max_sources = analytics.unique_sources(test_grb_1,
                                                           return_max_fan_out=True,
                                                           return_max_sources=True
                                                           )
    assert max_fan_out == 4
    assert max_sources == 1  # [0]


def test_unique_destinations():
    n_unique_destinations = analytics.unique_destinations(test_grb_1)
    assert n_unique_destinations == 4


def test_unique_destinations_return_max_fan_in():
    _, max_fan_in = analytics.unique_destinations(test_grb_1, return_max_fan_in=True)
    assert max_fan_in == 4


def test_unique_destinations_return_max_destinations():
    _, max_destinations = analytics.unique_destinations(test_grb_1, return_max_destinations=True)
    assert max_destinations == 2  # [2, 4]


def test_unique_destinations_return_max_fan_in_and_destinations():
    _, max_fan_in, max_destinations = analytics.unique_destinations(test_grb_1,
                                                                    return_max_fan_in=True,
                                                                    return_max_destinations=True
                                                                    )
    assert max_fan_in == 4
    assert max_destinations == 2  # [2, 4]
