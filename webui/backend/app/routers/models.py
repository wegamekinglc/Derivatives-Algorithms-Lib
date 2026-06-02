"""Model definition endpoints (Black-Scholes / Dupire)."""

from __future__ import annotations

from typing import List

from fastapi import APIRouter, Depends, HTTPException

from app.dependencies import store_dependency
from app.schemas import ModelCreate, ModelDefinition
from app.services.store import NotFoundError, Store

router = APIRouter(prefix="/api/models", tags=["models"])


@router.get("", response_model=List[ModelDefinition])
def list_models(store: Store = Depends(store_dependency)) -> List[ModelDefinition]:
    return store.list_models()


@router.post("", response_model=ModelDefinition, status_code=201)
def create_model(
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
def get_model(
    model_id: str,
    store: Store = Depends(store_dependency),
) -> ModelDefinition:
    try:
        return store.get_model(model_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.delete("/{model_id}", status_code=204)
def delete_model(model_id: str, store: Store = Depends(store_dependency)) -> None:
    store.delete_model(model_id)
