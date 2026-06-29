"""Model definition endpoints (Black-Scholes / Dupire)."""

from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException

from app.dependencies import store_dependency
from app.schemas import ModelCreate, ModelDefinition, ModelUpdate
from app.services.store import ConflictError, NotFoundError, Store

router = APIRouter(prefix="/api/models", tags=["models"])


@router.get("", response_model=list[ModelDefinition])
async def list_models(
    store: Store = Depends(store_dependency),
) -> list[ModelDefinition]:
    return store.list_models()


@router.post("", response_model=ModelDefinition, status_code=201)
async def create_model(
    payload: ModelCreate,
    store: Store = Depends(store_dependency),
) -> ModelDefinition:
    model = ModelDefinition(**payload.model_dump())
    try:
        model.dal_kind_and_params()  # validate params match the kind
    except ValueError as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc
    return store.add_model(model)


@router.get("/{model_id}", response_model=ModelDefinition)
async def get_model(
    model_id: str,
    store: Store = Depends(store_dependency),
) -> ModelDefinition:
    try:
        return store.get_model(model_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.put("/{model_id}", response_model=ModelDefinition)
async def update_model(
    model_id: str,
    payload: ModelUpdate,
    store: Store = Depends(store_dependency),
) -> ModelDefinition:
    patch = payload.model_dump(exclude_none=True)
    try:
        return store.update_model(model_id, patch)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    except ValueError as exc:
        # model.dal_kind_and_params() validation failure (e.g. kind=BS but no bs params)
        raise HTTPException(status_code=422, detail=str(exc)) from exc


@router.delete("/{model_id}", status_code=204)
async def delete_model(
    model_id: str, store: Store = Depends(store_dependency)
) -> None:
    try:
        store.delete_model(model_id)
    except ConflictError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
