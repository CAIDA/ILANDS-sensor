import graphblas as gb
import analysis as analysis

test_grb = gb.Matrix.from_coo([0, 0, 0, 0, 2, 2, 2, 3, 3, 3, 4, 4],
                              [1, 2, 3, 4, 1, 2, 4, 2, 3, 4, 2, 4],
                              [1, 1, 2, 1, 4, 2, 1, 2, 4, 1, 1, 1],
                              dtype=int,
                              nrows=2**32,
                              ncols=2**32
                              )

test_range_1 = gb.Matrix.from_coo([0, 2], [0, 2], [1, 1], dtype=int, nrows=2**32, ncols=2**32)
test_range_2 = gb.Matrix.from_coo([1, 4, 5], [1, 4, 5], [1, 1, 1], dtype=int, nrows=2**32, ncols=2**32)


def test_get_matrix_from_grb():
    my_test_matrix_from_grb = analysis.get_matrix_from_grb("tests/test_grb.grb")
    assert test_grb.isequal(my_test_matrix_from_grb)


def test_process_tarball():
    processed_grb = analysis.process_tarball("tests/my_test_tar.tar")
    assert test_grb.isequal(processed_grb)


def test_process_ranges_tarball():
    test_ranges = analysis.process_ranges_tarball("tests/test_ranges.tar")
    assert test_range_1.isequal(test_ranges[0])
    assert test_range_2.isequal(test_ranges[1])


def test_analysis_with_ranges():
    my_traffic_matrix = analysis.TrafficMatrix(test_grb,
                                               source_ranges=[test_range_1, test_range_2],
                                               destination_ranges=[test_range_1, test_range_2]
                                               )
    analysis_results = my_traffic_matrix.analyze()
    print(analysis_results)
    assert analysis_results["n_links"] == "12"
    assert analysis_results["n_packets"] == "21"
    assert analysis_results["n_sources"] == "4"
    assert analysis_results["n_destinations"] == "4"
    assert analysis_results["max_packets"] == "4"
    assert analysis_results["max_source_packets"] == "7"
    assert analysis_results["max_destination_packets"] == "6"
    assert analysis_results["max_fan_out"] == "4"
    assert analysis_results["max_fan_in"] == "4"
    assert analysis_results["sub_n_links"] == "[[2 4] [1 1]]"
    assert analysis_results["sub_n_packets"] == "[[3 7] [1 1]]"
    assert analysis_results["sub_n_sources"] == "[[2 2] [1 1]]"
    assert analysis_results["sub_n_destinations"] == "[[1 2] [1 1]]"
    assert analysis_results["sub_max_packets"] == "[[2 4] [1 1]]"
    assert analysis_results["sub_max_source_packets"] == "[[2 5] [1 1]]"
    assert analysis_results["sub_max_destination_packets"] == "[[3 5] [1 1]]"
    assert analysis_results["sub_max_fan_out"] == "[[1 2] [1 1]]"
    assert analysis_results["sub_max_fan_in"] == "[[2 2] [1 1]]"
