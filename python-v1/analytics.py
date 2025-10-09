import graphblas as gb
from typing import Union, Tuple


def valid_packets(traffic_matrix: gb.Matrix,
                  return_max_packets: bool = False,
                  return_max_IP_pairs: bool = False
                  ) \
        -> Union[int, Tuple[int, int], Tuple[int, list[Tuple[int, int]]], Tuple[int, int, int]]:
    total_packets = traffic_matrix.reduce_scalar(gb.monoid.plus).value
    total_packets = 0 if total_packets is None else int(total_packets)

    if return_max_IP_pairs:
        if total_packets == 0:
            max_packets = 0
            max_IP_pairs = 0
        else:
            max_packets = traffic_matrix.reduce_scalar(gb.monoid.max).value
            max_IP_pairs = int(gb.select.valueeq(traffic_matrix, max_packets).new().nvals)
        if return_max_packets:
            return total_packets, max_packets, max_IP_pairs
        else:
            return total_packets, max_IP_pairs
    else:
        if return_max_packets:
            max_packets = traffic_matrix.reduce_scalar(gb.monoid.max).value
            max_packets = 0 if max_packets is None else int(max_packets)
            return total_packets, max_packets
        else:
            return total_packets


def max_packets_by_sources(traffic_matrix: gb.Matrix,
                           return_max_sources: bool = False
                           ) \
        -> Union[int, Tuple[int, int]]:
    nonzero_rows = traffic_matrix.reduce_rowwise(gb.monoid.plus)
    max_packet_count = nonzero_rows.reduce(gb.monoid.max).value
    max_packet_count = 0 if max_packet_count is None else int(max_packet_count)
    if return_max_sources:
        if max_packet_count == 0:
            max_sources = 0
        else:
            max_sources = int(gb.select.valueeq(nonzero_rows, max_packet_count).new().nvals)
        return max_packet_count, max_sources
    else:
        return max_packet_count


def max_packets_by_destinations(traffic_matrix: gb.Matrix,
                                return_max_destinations: bool = False
                                ) \
        -> Union[int, Tuple[int, int]]:
    nonzero_columns = traffic_matrix.reduce_columnwise(gb.monoid.plus)
    max_packet_count = nonzero_columns.reduce(gb.monoid.max).value
    max_packet_count = 0 if max_packet_count is None else int(max_packet_count)
    if return_max_destinations:
        if max_packet_count is None:
            max_destinations = 0
        else:
            max_destinations = int(gb.select.valueeq(nonzero_columns, max_packet_count).new().nvals)
        return max_packet_count, max_destinations
    else:

        return max_packet_count


def unique_links(traffic_matrix: gb.Matrix) -> int:
    links = traffic_matrix.reduce_scalar(gb.agg.count).value
    links = 0 if links is None else int(links)
    return links


def unique_sources(traffic_matrix: gb.Matrix,
                   return_max_fan_out: bool = False,
                   return_max_sources: bool = False
                   ) \
        -> Union[int, Tuple[int, int], Tuple[int, int], Tuple[int, int, int]]:
    nonzero_rows = traffic_matrix.reduce_rowwise(gb.agg.count)
    n_sources = nonzero_rows.reduce(gb.agg.count).value
    n_sources = 0 if n_sources is None else int(n_sources)
    if return_max_sources:
        if n_sources == 0:
            max_fan_out = 0
            max_sources = 0
        else:
            max_fan_out = int(nonzero_rows.reduce(gb.monoid.max).value)
            max_sources = int(gb.select.valueeq(nonzero_rows, max_fan_out).new().nvals)
        if return_max_fan_out:
            return n_sources, max_fan_out, max_sources
        else:
            return n_sources, max_sources
    else:
        if return_max_fan_out:
            max_fan_out = nonzero_rows.reduce(gb.monoid.max).value
            max_fan_out = 0 if max_fan_out is None else int(max_fan_out)
            return n_sources, max_fan_out
        else:
            return n_sources


def unique_destinations(traffic_matrix: gb.Matrix,
                        return_max_fan_in: bool = False,
                        return_max_destinations: bool = False
                        ) \
        -> Union[int, Tuple[int, int], Tuple[int, int], Tuple[int, int, int]]:
    nonzero_columns = traffic_matrix.reduce_columnwise(gb.agg.count)
    n_destinations = nonzero_columns.reduce(gb.agg.count).value
    n_destinations = 0 if n_destinations is None else int(n_destinations)
    if return_max_destinations:
        if n_destinations == 0:
            max_fan_in = 0
            max_destinations = 0
        else:
            max_fan_in = int(nonzero_columns.reduce(gb.monoid.max).value)
            max_destinations = int(gb.select.valueeq(nonzero_columns, max_fan_in).new().nvals)
        if return_max_fan_in:
            return n_destinations, max_fan_in, max_destinations
        else:
            return n_destinations, max_destinations
    else:
        if return_max_fan_in:
            max_fan_in = int(nonzero_columns.reduce(gb.monoid.max).value)
            return n_destinations, max_fan_in
        else:
            return n_destinations
