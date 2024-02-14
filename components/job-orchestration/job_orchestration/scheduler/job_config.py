from __future__ import annotations

import typing

from pydantic import BaseModel


class PathsToCompress(BaseModel):
    file_paths: typing.List[str]
    group_ids: typing.List[int]
    st_sizes: typing.List[int]
    empty_directories: typing.List[str] = None


class InputConfig(BaseModel):
    list_path: str
    path_prefix_to_remove: str = None
    timestamp_key: typing.Optional[str] = None


class OutputConfig(BaseModel):
    tags: typing.Optional[typing.List[str]] = None
    target_archive_size: int
    target_dictionaries_size: int
    target_segment_size: int
    target_encoded_file_size: int


class ClpIoConfig(BaseModel):
    input: InputConfig
    output: OutputConfig


class SearchConfig(BaseModel):
    query_string: str
    begin_timestamp: typing.Optional[int] = None
    end_timestamp: typing.Optional[int] = None
    ignore_case: bool = False
    path_filter: typing.Optional[str] = None
