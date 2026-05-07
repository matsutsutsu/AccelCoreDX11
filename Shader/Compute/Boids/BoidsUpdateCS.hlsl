//#include "BoidsData.hlsli"

//RWStructuredBuffer<BoidData> BoidsBuffer : register(u0);

//[numthreads(256, 1, 1)]
//void main(uint3 DTid : SV_DispatchThreadID)
//{
//    if (DTid.x >= (uint) BoidsCount)
//        return;

//    BoidData me = BoidsBuffer[DTid.x];

//    float3 separation = float3(0, 0, 0);
//    float3 alignment = float3(0, 0, 0);
//    float3 cohesion = float3(0, 0, 0);
//    int neighborCount = 0;

//    for (int i = 0; i < BoidsCount; ++i)
//    {
//        if (i == (int) DTid.x)
//            continue;

//        BoidData other = BoidsBuffer[i];
//        float dist = distance(me.position, other.position);

//        if (dist > 0 && dist < PerceptionRadius)
//        {
//            separation += normalize(me.position - other.position) / dist;
//            alignment += other.velocity;
//            cohesion += other.position;
//            neighborCount++;
//        }
//    }

//    float3 totalForce = float3(0, 0, 0);

//    if (neighborCount > 0)
//    {
//        alignment = normalize(alignment / neighborCount) * MaxSpeed;
//        float3 steerA = alignment - me.velocity;
        
//        cohesion = (cohesion / neighborCount) - me.position;
//        cohesion = normalize(cohesion) * MaxSpeed;
//        float3 steerC = cohesion - me.velocity;

//        totalForce += separation * SeparationWeight;
//        totalForce += steerA * AlignmentWeight;
//        totalForce += steerC * CohesionWeight;
//    }

//    if (TargetWeight > 0.0f)
//    {
//        float3 toTarget = TargetPosition - me.position;
//        float3 steerT = normalize(toTarget) * MaxSpeed - me.velocity;
//        totalForce += steerT * TargetWeight;
//    }

//    me.velocity += totalForce * DeltaTime;
    
//    if (length(me.velocity) > MaxSpeed)
//    {
//        me.velocity = normalize(me.velocity) * MaxSpeed;
//    }

//    me.position += me.velocity * DeltaTime;
//    BoidsBuffer[DTid.x] = me;
//}