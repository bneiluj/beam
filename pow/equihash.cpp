#include "core/common.h"
#include "impl/crypto/equihash.h"
#include "impl/uint256.h"
#include "impl/arith_uint256.h"
#include <utility>

namespace beam
{

struct Block::PoW::Helper
{
	blake2b_state m_Blake;
	Equihash<Block::PoW::N, Block::PoW::K> m_Eh;

	void Reset(const void* pInput, uint32_t nSizeInput, const NonceType& nonce)
	{
		m_Eh.InitialiseState(m_Blake);

		// H(I||...
		blake2b_update(&m_Blake, (uint8_t*) pInput, nSizeInput);
		blake2b_update(&m_Blake, nonce.m_pData, sizeof(nonce.m_pData));
	}

	bool TestDifficulty(const uint8_t* pSol, uint32_t nSol, Difficulty d) const
	{
		ECC::Hash::Value hv;

		blake2b_state b2s = m_Blake;
		blake2b_update(&b2s, pSol, nSol);
		blake2b_final(&b2s, hv.m_pData, sizeof(hv.m_pData));

		return d.IsTargetReached(hv);
	}
};

bool Block::PoW::Solve(const void* pInput, uint32_t nSizeInput, const Cancel& fnCancel)
{
	Helper hlp;

	std::function<bool(const beam::ByteBuffer&)> fnValid = [this, &hlp](const beam::ByteBuffer& solution)
		{
			if (!hlp.TestDifficulty(&solution.front(), (uint32_t) solution.size(), m_Difficulty))
				return false;
			assert(solution.size() == m_Indices.size());
            std::copy(solution.begin(), solution.end(), m_Indices.begin());
            return true;
        };


    std::function<bool(EhSolverCancelCheck)> fnCancelInternal = [fnCancel](EhSolverCancelCheck pos) {
        return fnCancel(false);
    };

    while (true)
    {
		hlp.Reset(pInput, nSizeInput, m_Nonce);

		try {

			if (hlp.m_Eh.OptimisedSolve(hlp.m_Blake, fnValid, fnCancelInternal))
				break;

		} catch (const EhSolverCancelledException&) {
			return false;
		}

		if (fnCancel(true))
			return false; // retry not allowed

        m_Nonce.Inc();
    }

    return true;
}

bool Block::PoW::IsValid(const void* pInput, uint32_t nSizeInput) const
{
	Helper hlp;
	hlp.Reset(pInput, nSizeInput, m_Nonce);

	std::vector<uint8_t> v(m_Indices.begin(), m_Indices.end());
    return
		hlp.m_Eh.IsValidSolution(hlp.m_Blake, v) &&
		hlp.TestDifficulty(&m_Indices.front(), (uint32_t) m_Indices.size(), m_Difficulty);
}

} // namespace beam

