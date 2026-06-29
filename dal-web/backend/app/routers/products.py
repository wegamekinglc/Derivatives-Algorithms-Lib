"""Product definition + builder template endpoints."""

from __future__ import annotations

import asyncio

from fastapi import APIRouter, Depends, HTTPException

from app.dependencies import gateway_dependency, store_dependency
from app.schemas import (
    ProductCreate,
    ProductDebugRequest,
    ProductDebugResponse,
    ProductDefinition,
    ProductUpdate,
)
from app.services.dal_gateway import DalGateway
from app.services.store import ConflictError, NotFoundError, Store
from app.services.templates import product_templates

router = APIRouter(prefix="/api/products", tags=["products"])


@router.get("/templates")
async def list_templates() -> list:
    return product_templates()


@router.get("", response_model=list[ProductDefinition])
async def list_products(
    store: Store = Depends(store_dependency),
) -> list[ProductDefinition]:
    return store.list_products()


@router.post("", response_model=ProductDefinition, status_code=201)
async def create_product(
    payload: ProductCreate,
    store: Store = Depends(store_dependency),
) -> ProductDefinition:
    product = ProductDefinition(**payload.model_dump())
    return store.add_product(product)


@router.get("/{product_id}", response_model=ProductDefinition)
async def get_product(
    product_id: str,
    store: Store = Depends(store_dependency),
) -> ProductDefinition:
    try:
        return store.get_product(product_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.put("/{product_id}", response_model=ProductDefinition)
async def update_product(
    product_id: str,
    payload: ProductUpdate,
    store: Store = Depends(store_dependency),
) -> ProductDefinition:
    patch = payload.model_dump(exclude_none=True)
    try:
        return store.update_product(product_id, patch)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.delete("/{product_id}", status_code=204)
async def delete_product(
    product_id: str, store: Store = Depends(store_dependency)
) -> None:
    try:
        store.delete_product(product_id)
    except ConflictError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc


@router.post("/debug", response_model=ProductDebugResponse)
async def debug_product(
    payload: ProductDebugRequest,
    gateway: DalGateway = Depends(gateway_dependency),
) -> ProductDebugResponse:
    """Render a DAL script-product debug dump for a set of event rows."""
    dates = [r.to_event_date_token() for r in payload.rows]
    events = [r.event for r in payload.rows]
    try:
        debug = await asyncio.to_thread(gateway.debug_product, dates, events)
    except (ValueError, RuntimeError) as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    return ProductDebugResponse(debug=debug)
